#include "dss/ui/view_model/display_view_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "dss/app/service_keys.h"
#include "dss/processing/frame/display_stretch.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/ui/support/qt_thread_utils.h"

namespace Dss::Ui {
namespace {

constexpr int kMinDisplayStretchValue = 0;      ///< 显示拉伸最小阈值。
constexpr int kMaxDisplayStretchValue = 16384;  ///< 显示拉伸最大阈值。

/// @brief 判断显示拉伸阈值是否位于 UI 支持范围内。
[[nodiscard]] auto isDisplayStretchValueInRange(int value) -> bool {
    return value >= kMinDisplayStretchValue && value <= kMaxDisplayStretchValue;
}

/// @brief 按 UI 状态生成显示拉伸设置。
[[nodiscard]] auto makeDisplayStretchSettings(bool autoStretch, int low, int high)
    -> Dss::Processing::DisplayStretchSettings {
    Dss::Processing::DisplayStretchSettings settings{};
    settings.mode = autoStretch ? Dss::Processing::DisplayStretchMode::Auto
                                : Dss::Processing::DisplayStretchMode::Manual;
    settings.low = static_cast<std::uint16_t>(low);
    settings.high = static_cast<std::uint16_t>(high);
    return settings;
}

/// @brief 将灰度显示缓冲复制为独立 QImage。
[[nodiscard]] auto makeGrayImageCopy(const std::vector<std::uint8_t>& displayImage,
                                     std::uint32_t width, std::uint32_t height,
                                     std::uint32_t stride) -> QImage {
    QImage image(displayImage.data(), static_cast<int>(width), static_cast<int>(height),
                 static_cast<qsizetype>(stride), QImage::Format_Grayscale8);
    return image.copy();
}

}  // namespace

DisplayViewModel::DisplayViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_bus(context.bus), m_registry(context.registry) {
    setupSubscriptions();
}

DisplayViewModel::~DisplayViewModel() = default;

bool DisplayViewModel::displayAutoStretch() const {
    return m_displayAutoStretch;
}

int DisplayViewModel::displayStretchLow() const {
    return m_displayStretchLow;
}

int DisplayViewModel::displayStretchHigh() const {
    return m_displayStretchHigh;
}

bool DisplayViewModel::rawDisplayEnabled() const {
    return m_rawDisplayEnabled;
}

void DisplayViewModel::toggleZoom(int level) {
    m_bus.emit(Dss::Core::ZoomChangeEvent{level});
}

bool DisplayViewModel::applyDisplayStretch(bool autoStretch, int low, int high) {
    if (!isDisplayStretchValueInRange(low) || !isDisplayStretchValueInRange(high) || high <= low) {
        Q_EMIT statusTextChanged("Display stretch must satisfy 0 <= low < high <= 16384");
        return false;
    }

    const auto changed = m_displayAutoStretch != autoStretch || m_displayStretchLow != low ||
                         m_displayStretchHigh != high;
    const auto wasAutoStretch = m_displayAutoStretch;
    m_displayAutoStretch = autoStretch;
    m_displayStretchLow = low;
    m_displayStretchHigh = high;
    if (!syncDisplayStretchToProcessor()) {
        return false;
    }
    if (changed) {
        Q_EMIT displayStretchSettingsChanged();
        Q_EMIT displayStretchChanged(m_displayAutoStretch, m_displayStretchLow,
                                     m_displayStretchHigh);
        if (m_rawDisplayEnabled) {
            if ((m_displayAutoStretch || wasAutoStretch) && emitCurrentRawDisplayFrame()) {
                return true;
            }
            if (!m_displayAutoStretch) {
                return true;
            }
        }
        (void)refreshCurrentDisplayFromStretch();
    }
    return true;
}

void DisplayViewModel::setDisplayAutoStretch(bool autoStretch) {
    (void)applyDisplayStretch(autoStretch, m_displayStretchLow, m_displayStretchHigh);
}

void DisplayViewModel::setDisplayStretchLow(int low) {
    const auto boundedLow = std::clamp(low, kMinDisplayStretchValue, kMaxDisplayStretchValue - 1);
    const auto normalizedLow = std::min(boundedLow, m_displayStretchHigh - 1);
    (void)applyDisplayStretch(m_displayAutoStretch, normalizedLow, m_displayStretchHigh);
}

void DisplayViewModel::setDisplayStretchHigh(int high) {
    const auto boundedHigh = std::clamp(high, kMinDisplayStretchValue + 1, kMaxDisplayStretchValue);
    const auto normalizedHigh = std::max(boundedHigh, m_displayStretchLow + 1);
    (void)applyDisplayStretch(m_displayAutoStretch, m_displayStretchLow, normalizedHigh);
}

void DisplayViewModel::setRawDisplayEnabled(bool enabled) {
    if (m_rawDisplayEnabled == enabled) {
        return;
    }
    m_rawDisplayEnabled = enabled;
    if (auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>(
            Dss::App::ServiceKey::imageProcessor)) {
        processor->setCpuDisplayImageRequired(!enabled);
    }
    if (m_rawDisplayEnabled) {
        (void)emitCurrentRawDisplayFrame();
    } else {
        (void)refreshCurrentDisplayFromStretch();
    }
}

auto DisplayViewModel::resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
    std::scoped_lock lock(m_pendingMutex, m_currentDisplayMutex);
    auto result = m_resources;
    if (m_pendingFrame) {
        result.queuedItems = 1;
        result.queuedBytes =
            (m_pendingFrame->rawImage ? m_pendingFrame->rawImage->capacity() * sizeof(std::uint16_t)
                                      : 0) +
            (m_pendingFrame->displayImage ? m_pendingFrame->displayImage->capacity() : 0);
    }
    // active 表示当前 RAW 缓存，不包括 QImage、纹理或正在发送的 UI 信号。
    result.activeItems = m_currentRawImage ? 1 : 0;
    result.activeBytes =
        m_currentRawImage ? m_currentRawImage->capacity() * sizeof(std::uint16_t) : 0;
    return result;
}

void DisplayViewModel::clearCurrentDisplayFrame() {
    std::scoped_lock lock(m_pendingMutex, m_currentDisplayMutex);
    ++m_sessionRevision;
    m_pendingFrame.reset();
    m_pendingStats.reset();
    // 保留 m_flushQueued：已投递的唤醒仍会执行，可处理随后到达的新帧。
    m_currentRawImage.reset();
    m_currentDisplayFrameSeq = 0;
    m_currentDisplayWidth = 0;
    m_currentDisplayHeight = 0;
    m_currentDisplayStats = {};
    m_currentAutoStretchLow = 0;
    m_currentAutoStretchHigh = 1;
    m_currentAutoStretchWindowValid = false;
}

void DisplayViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::ProcessingSessionResetEvent>(
        [this](const auto&) { clearCurrentDisplayFrame(); }));
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [this](const Dss::Core::ProcessingCompleteEvent& e) { onProcessingComplete(e); }));
}

void DisplayViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    if (!isObjectThread(this)) {
        {
            std::lock_guard lock(m_pendingMutex);
            if (m_pendingFrame) {
                ++m_resources.replacedItems;
            }
            m_pendingFrame = event;
            const auto bytes =
                (event.rawImage ? event.rawImage->capacity() * sizeof(std::uint16_t) : 0) +
                (event.displayImage ? event.displayImage->capacity() : 0);
            m_resources.peakQueuedBytes =
                (std::max)(m_resources.peakQueuedBytes, static_cast<std::uint64_t>(bytes));
            if (m_flushQueued) {
                return;
            }
            m_flushQueued = true;
        }
        invokeOnObjectThread(this, [this] { flushPendingUpdates(); });
        return;
    }

    displayFrame(event, m_sessionRevision.load());
}

void DisplayViewModel::displayFrame(const Dss::Core::DisplayRefreshEvent& event,
                                    std::uint64_t revision) {
    if (event.width == 0 || event.height == 0 || event.stride == 0 ||
        !cacheCurrentDisplayFrame(event, revision)) {
        return;
    }
    if (m_rawDisplayEnabled && emitCurrentRawDisplayFrame()) {
        return;
    }

    if (!event.displayImage) {
        return;
    }
    const auto expectedSize =
        static_cast<std::size_t>(event.stride) * static_cast<std::size_t>(event.height);
    if (event.displayImage->size() < expectedSize) {
        return;
    }
    auto image = makeGrayImageCopy(*event.displayImage, event.width, event.height, event.stride);
    if (revision == m_sessionRevision.load()) {
        Q_EMIT displayImageReady(std::move(image));
    }
}

void DisplayViewModel::onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event) {
    if (!isObjectThread(this)) {
        {
            std::lock_guard lock(m_pendingMutex);
            m_pendingStats = event;
            if (m_flushQueued) {
                return;
            }
            m_flushQueued = true;
        }
        invokeOnObjectThread(this, [this] { flushPendingUpdates(); });
        return;
    }

    displayStats(event.stats, m_sessionRevision.load());
}

void DisplayViewModel::displayStats(const Dss::Core::ImageStats& stats, std::uint64_t revision) {
    if (revision == m_sessionRevision.load()) {
        Q_EMIT imageStatsUpdated(stats.minVal, stats.maxVal, stats.avg, stats.stdDev);
    }
}

void DisplayViewModel::flushPendingUpdates() {
    std::optional<Dss::Core::DisplayRefreshEvent> frame;
    std::optional<Dss::Core::ProcessingCompleteEvent> stats;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock(m_pendingMutex);
        frame = std::exchange(m_pendingFrame, std::nullopt);
        stats = std::exchange(m_pendingStats, std::nullopt);
        revision = m_sessionRevision.load();
        m_flushQueued = false;
    }
    if (frame) {
        displayFrame(*frame, revision);
    }
    if (stats) {
        displayStats(stats->stats, revision);
    }
}

bool DisplayViewModel::cacheCurrentDisplayFrame(const Dss::Core::DisplayRefreshEvent& event,
                                                std::uint64_t revision) {
    const auto expectedPixelCount =
        static_cast<std::size_t>(event.width) * static_cast<std::size_t>(event.height);

    std::scoped_lock lock(m_pendingMutex, m_currentDisplayMutex);
    if (revision != m_sessionRevision.load()) {
        return false;
    }
    m_currentDisplayFrameSeq = event.frameSeq;
    m_currentDisplayWidth = event.width;
    m_currentDisplayHeight = event.height;
    m_currentDisplayStats = event.stats;
    m_currentAutoStretchLow = static_cast<int>(event.displayStretchLow);
    m_currentAutoStretchHigh = static_cast<int>(event.displayStretchHigh);
    m_currentAutoStretchWindowValid = event.displayStretchWindowValid;
    if (event.rawImage && event.rawImage->size() == expectedPixelCount) {
        m_currentRawImage = event.rawImage;
    } else {
        m_currentRawImage.reset();
        m_currentAutoStretchWindowValid = false;
    }
    ++m_resources.completedItems;
    return true;
}

bool DisplayViewModel::emitCurrentRawDisplayFrame() {
    std::shared_ptr<const std::vector<std::uint16_t>> rawImage;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int low = m_displayStretchLow;
    int high = m_displayStretchHigh;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock(m_currentDisplayMutex);
        revision = m_sessionRevision.load();
        rawImage = m_currentRawImage;
        width = m_currentDisplayWidth;
        height = m_currentDisplayHeight;
        if (m_displayAutoStretch) {
            if (!m_currentAutoStretchWindowValid) {
                return false;
            }
            low = m_currentAutoStretchLow;
            high = m_currentAutoStretchHigh;
        }
    }
    const auto expectedPixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (!rawImage || width == 0 || height == 0 || rawImage->size() != expectedPixelCount) {
        return false;
    }
    if (high <= low) {
        high = low + 1;
    }

    if (revision != m_sessionRevision.load()) {
        return false;
    }
    Q_EMIT rawDisplayFrameReady(std::move(rawImage), width, height, width, low, high);
    return true;
}

bool DisplayViewModel::refreshCurrentDisplayFromStretch() {
    std::shared_ptr<const std::vector<std::uint16_t>> rawImage;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock(m_currentDisplayMutex);
        revision = m_sessionRevision.load();
        rawImage = m_currentRawImage;
        width = m_currentDisplayWidth;
        height = m_currentDisplayHeight;
    }
    if (!rawImage || width == 0 || height == 0) {
        return false;
    }

    const auto expectedPixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (rawImage->size() != expectedPixelCount) {
        return false;
    }

    std::vector<std::uint8_t> displayImage;
    const auto settings =
        makeDisplayStretchSettings(m_displayAutoStretch, m_displayStretchLow, m_displayStretchHigh);
    if (settings.mode == Dss::Processing::DisplayStretchMode::Manual) {
        const auto window = Dss::Processing::resolveDisplayStretchWindow({}, settings);
        displayImage = Dss::Processing::stretchDisplayImage(*rawImage, window);
    } else {
        auto display = Dss::Processing::buildDisplayImage(*rawImage, settings);
        displayStats(display.stats, revision);
        displayImage = std::move(display.displayImage);
    }

    if (displayImage.size() != expectedPixelCount) {
        return false;
    }
    auto image = makeGrayImageCopy(displayImage, width, height, width);
    if (revision != m_sessionRevision.load()) {
        return false;
    }
    Q_EMIT displayImageReady(std::move(image));
    return true;
}

bool DisplayViewModel::syncDisplayStretchToProcessor() {
    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    if (!processor) {
        Q_EMIT statusTextChanged("Image processor is not registered");
        return false;
    }

    processor->setDisplayStretchSettings(makeDisplayStretchSettings(
        m_displayAutoStretch, m_displayStretchLow, m_displayStretchHigh));
    return true;
}

}  // namespace Dss::Ui
