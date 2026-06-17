#include "dss/ui/display_view_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "dss/processing/display_stretch.h"
#include "dss/processing/image_processor.h"

namespace Dss::Ui {
namespace {

constexpr int kMinDisplayStretchValue = 0;       ///< 显示拉伸最小阈值。
constexpr int kMaxDisplayStretchValue = 16384;   ///< 显示拉伸最大阈值。

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

void DisplayViewModel::clearCurrentDisplayFrame() {
    std::lock_guard lock(m_currentDisplayMutex);
    m_currentRawImage.reset();
    m_currentDisplayFrameSeq = 0;
    m_currentDisplayWidth = 0;
    m_currentDisplayHeight = 0;
}

void DisplayViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [this](const Dss::Core::ProcessingCompleteEvent& e) { onProcessingComplete(e); }));
}

void DisplayViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    if (!event.displayImage || event.width == 0 || event.height == 0 || event.stride == 0) {
        return;
    }

    const auto expectedSize =
        static_cast<std::size_t>(event.stride) * static_cast<std::size_t>(event.height);
    if (event.displayImage->size() < expectedSize) {
        return;
    }

    cacheCurrentDisplayFrame(event);
    Q_EMIT displayImageReady(
        makeGrayImageCopy(*event.displayImage, event.width, event.height, event.stride));
}

void DisplayViewModel::onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event) {
    const auto& stats = event.stats;
    Q_EMIT imageStatsUpdated(stats.minVal, stats.maxVal, stats.avg, stats.stdDev);
}

void DisplayViewModel::cacheCurrentDisplayFrame(const Dss::Core::DisplayRefreshEvent& event) {
    const auto expectedPixelCount =
        static_cast<std::size_t>(event.width) * static_cast<std::size_t>(event.height);

    std::lock_guard lock(m_currentDisplayMutex);
    m_currentDisplayFrameSeq = event.frameSeq;
    m_currentDisplayWidth = event.width;
    m_currentDisplayHeight = event.height;
    if (event.rawImage && event.rawImage->size() == expectedPixelCount) {
        m_currentRawImage = event.rawImage;
    } else {
        m_currentRawImage.reset();
    }
}

bool DisplayViewModel::refreshCurrentDisplayFromStretch() {
    std::shared_ptr<const std::vector<std::uint16_t>> rawImage;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    {
        std::lock_guard lock(m_currentDisplayMutex);
        rawImage = m_currentRawImage;
        width = m_currentDisplayWidth;
        height = m_currentDisplayHeight;
    }
    if (!rawImage || width == 0 || height == 0) {
        return false;
    }

    const auto expectedPixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
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
        Q_EMIT imageStatsUpdated(display.stats.minVal, display.stats.maxVal, display.stats.avg,
                                 display.stats.stdDev);
        displayImage = std::move(display.displayImage);
    }

    if (displayImage.size() != expectedPixelCount) {
        return false;
    }
    Q_EMIT displayImageReady(makeGrayImageCopy(displayImage, width, height, width));
    return true;
}

bool DisplayViewModel::syncDisplayStretchToProcessor() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        Q_EMIT statusTextChanged("Image processor is not registered");
        return false;
    }

    processor->setDisplayStretchSettings(
        makeDisplayStretchSettings(m_displayAutoStretch, m_displayStretchLow, m_displayStretchHigh));
    return true;
}

}  // namespace Dss::Ui
