#include "dss/processing/pipeline/image_processor.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

#include "dss/core/concurrency/background_task.h"
#include "dss/core/event/events.h"

namespace Dss::Processing {

namespace {
/// 在异常离开 worker 时先标记失败，再由顶层 runBackgroundTask 发布诊断。
struct WorkerFailureGuard {
    explicit WorkerFailureGuard(std::atomic<bool>& flag) noexcept : failed(flag) {}
    WorkerFailureGuard(const WorkerFailureGuard&) = delete;
    auto operator=(const WorkerFailureGuard&) -> WorkerFailureGuard& = delete;
    WorkerFailureGuard(WorkerFailureGuard&&) = delete;
    auto operator=(WorkerFailureGuard&&) -> WorkerFailureGuard& = delete;

    std::atomic<bool>& failed;                           ///< 工作线程的失败标志。
    int initialExceptions = std::uncaught_exceptions();  ///< 进入任务时正在传播的异常数。
    /// @brief 栈展开时标记失败，不捕获或重新抛出异常。
    ~WorkerFailureGuard() {
        if (std::uncaught_exceptions() > initialExceptions) {
            failed.store(true);
        }
    }
};
}  // namespace

ImageProcessor::ImageProcessor(MessageBus& bus) : m_bus(bus) {}

ImageProcessor::~ImageProcessor() {
    stop();
}

void ImageProcessor::start() {
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
    if (m_running.exchange(true)) {
        return;
    }
    m_frameChannel.open();
    m_failed.store(false);
    std::stop_source resultCancellation;
    {
        std::scoped_lock lock(m_resultMutex);
        m_resultCancellation = resultCancellation;
    }
    m_workerThread = std::jthread([this, resultCancellation](std::stop_token token) {
        const std::stop_callback cancelResults(
            token, [resultCancellation] mutable { resultCancellation.request_stop(); });
        Dss::Core::runBackgroundTask(m_bus, "image_processor", [this, token] {
            const WorkerFailureGuard failureGuard{m_failed};
            workerLoop(token);
        });
        m_frameChannel.close();
        m_frameChannel.clear();
        {
            std::scoped_lock lock(m_resourceMutex);
            m_resources.activeItems = 0;
            m_resources.activeBytes = 0;
        }
        m_running.store(false);
    });
}

void ImageProcessor::stop() {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
    m_running.store(false);
    m_frameChannel.close();
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_frameChannel.clear();
    std::scoped_lock resourceLock(m_resourceMutex);
    m_resources.lastStopMicroseconds = Dss::Core::elapsedMicroseconds(started);
}

bool ImageProcessor::submitFrame(FramePacket packet) {
    if (!m_running.load()) {
        return false;
    }
    const auto bytes = imagePayloadBytes(packet);
    if (!m_frameChannel.tryPush(std::move(packet), bytes)) {
        m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void ImageProcessor::drain() {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
    m_running.store(false);
    m_frameChannel.close();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    std::scoped_lock resourceLock(m_resourceMutex);
    m_resources.lastStopMicroseconds = Dss::Core::elapsedMicroseconds(started);
}

void ImageProcessor::resetSession() {
    stop();
    {
        std::scoped_lock lock(m_strategyMutex);
        m_pipeline.reset();
        if (m_trackStrategy) {
            m_trackStrategy->reset();
        }
    }
    m_bus.emit(Dss::Core::ProcessingSessionResetEvent{});
}

bool ImageProcessor::submitFrameBlocking(FramePacket packet, std::stop_token token) {
    if (!m_running.load() || token.stop_requested()) {
        return false;
    }
    const auto bytes = imagePayloadBytes(packet);
    return m_frameChannel.push(std::move(packet), token, bytes);
}

auto ImageProcessor::resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
    auto result = m_frameChannel.resourceSnapshot();
    std::scoped_lock lock(m_resourceMutex);
    result.activeItems = m_resources.activeItems;
    result.activeBytes = m_resources.activeBytes;
    result.completedItems = m_resources.completedItems;
    result.lastWorkMicroseconds = m_resources.lastWorkMicroseconds;
    result.maxWorkMicroseconds = m_resources.maxWorkMicroseconds;
    result.lastStopMicroseconds = m_resources.lastStopMicroseconds;
    return result;
}

auto ImageProcessor::droppedFrames() const -> uint64_t {
    return m_droppedFrames.load(std::memory_order_relaxed);
}

bool ImageProcessor::isRunning() const {
    return m_running.load();
}

void ImageProcessor::setTrackResultSink(TrackResultSink sink) {
    auto shared = sink ? std::make_shared<const TrackResultSink>(std::move(sink)) : nullptr;
    {
        std::scoped_lock lock(m_resultMutex);
        m_resultSink.swap(shared);
    }
}

void ImageProcessor::cancelPendingResults() {
    std::stop_source cancellation;
    {
        std::scoped_lock lock(m_resultMutex);
        cancellation = m_resultCancellation;
    }
    cancellation.request_stop();
}

void ImageProcessor::deliverTrackResult(const Dss::Core::TrackResultEvent& event,
                                        const FramePacket& packet) {
    std::shared_ptr<const TrackResultSink> sink;
    std::stop_token token;
    {
        std::scoped_lock lock(m_resultMutex);
        sink = m_resultSink;
        token = m_resultCancellation.get_token();
    }
    if (sink && !token.stop_requested()) {
        const auto result = (*sink)(event, packet, token);
        if (!result) {
            throw std::runtime_error(result.error());
        }
    }
}

void ImageProcessor::setProcessingStrategy(std::unique_ptr<IProcessingStrategy> strategy) {
    std::scoped_lock lock(m_strategyMutex);
    m_pipeline.setBackend(std::move(strategy));
}

void ImageProcessor::setTrackingStrategy(
    std::unique_ptr<Dss::Tracking::ITrackingStrategy> strategy) {
    std::scoped_lock lock(m_strategyMutex);
    m_trackStrategy = std::move(strategy);
}

auto ImageProcessor::currentProcessingMode() const -> Dss::Core::ProcessingMode {
    std::scoped_lock lock(m_strategyMutex);
    return m_pipeline.currentMode();
}

auto ImageProcessor::currentTrackMode() const -> Dss::Core::TrackMode {
    std::scoped_lock lock(m_strategyMutex);
    return m_trackStrategy ? m_trackStrategy->mode() : Dss::Core::TrackMode::Init;
}

void ImageProcessor::setDisplayStretchSettings(DisplayStretchSettings settings) {
    std::scoped_lock lock(m_displayStretchMutex);
    m_displayStretchSettings = settings;
}

auto ImageProcessor::displayStretchSettings() const -> DisplayStretchSettings {
    return currentDisplayStretchSettings();
}

void ImageProcessor::setCpuDisplayImageRequired(bool required) {
    m_cpuDisplayImageRequired.store(required);
}

auto ImageProcessor::currentDisplayStretchSettings() const -> DisplayStretchSettings {
    std::scoped_lock lock(m_displayStretchMutex);
    return m_displayStretchSettings;
}

void ImageProcessor::processFrame(FramePacket& packet) {
    packet.backendDisplayRequired = false;

    ProcessingResult procResult;
    {
        std::scoped_lock lock(m_strategyMutex);
        procResult = m_pipeline.process(packet);
    }

    auto tracks = trackFrame(packet, procResult);
    publishFrameResults(packet, procResult, std::move(tracks));
}

auto ImageProcessor::trackFrame(FramePacket& packet, ProcessingResult& procResult)
    -> std::optional<std::vector<Dss::Core::TargetInfo>> {
    std::scoped_lock lock(m_strategyMutex);
    if (!m_trackStrategy || (!procResult.success && m_pipeline.hasBackend())) {
        return std::nullopt;
    }
    Dss::Core::FrameMeasurements meas{};
    meas.frameSeq = packet.frameSeq;
    meas.timestamp = packet.metadata.timestamp;
    meas.fovCenterAe = packet.metadata.pointingAe;
    meas.exposureTime = packet.metadata.exposureTime;
    meas.frameFreq = packet.metadata.frameFrequency;
    meas.targetBlobs =
        procResult.success ? std::move(procResult.targetBlobs) : std::move(packet.targetBlobs);
    meas.validatedTargetBlobs = procResult.success ? std::move(procResult.validatedTargetBlobs)
                                                   : std::move(packet.validatedTargetBlobs);
    meas.starBlobs =
        procResult.success ? std::move(procResult.starBlobs) : std::move(packet.starBlobs);

    return m_trackStrategy->track(meas);
}

void ImageProcessor::publishFrameResults(
    FramePacket& packet, ProcessingResult& procResult,
    std::optional<std::vector<Dss::Core::TargetInfo>> trackResults) {
    auto displayStats = procResult.success ? procResult.stats : packet.stats;
    DisplayStretchWindow displayWindow{};
    bool displayWindowValid = false;
    std::vector<std::uint8_t> displayBuffer;
    const auto expectedPixelCount =
        static_cast<std::size_t>(packet.width) * static_cast<std::size_t>(packet.height);
    const auto rawImageValid =
        packet.rawImage && expectedPixelCount > 0U && packet.rawImage->size() == expectedPixelCount;
    if (rawImageValid) {
        const auto settings = currentDisplayStretchSettings();
        if (!procResult.success || !procResult.rawStatsValid) {
            displayStats = computeImageStats(*packet.rawImage);
        }
        displayWindow = resolveDisplayStretchWindow(displayStats, settings);
        displayWindowValid = true;
        if (m_cpuDisplayImageRequired.load()) {
            displayBuffer = stretchDisplayImage(*packet.rawImage, displayWindow);
        }
    } else if (!procResult.displayImage.empty()) {
        displayBuffer = std::move(procResult.displayImage);
    } else {
        displayBuffer = std::move(packet.displayImage);
    }

    auto displayImage = std::make_shared<const std::vector<std::uint8_t>>(std::move(displayBuffer));
    auto rawImage = rawImageValid ? packet.rawImage : SharedRawImage{};

    Dss::Core::DisplayRefreshEvent refreshEvent{};
    refreshEvent.frameSeq = packet.frameSeq;
    refreshEvent.width = packet.width;
    refreshEvent.height = packet.height;
    refreshEvent.stride = packet.width;
    refreshEvent.displayImage = displayImage;
    refreshEvent.rawImage = rawImage;
    refreshEvent.stats = displayStats;
    refreshEvent.displayStretchLow = displayWindow.low;
    refreshEvent.displayStretchHigh = displayWindow.high;
    refreshEvent.displayStretchWindowValid = displayWindowValid;
    m_bus.emit(refreshEvent);
    std::optional<Dss::Core::TrackResultEvent> trackEvent;
    if (trackResults) {
        trackEvent = Dss::Core::TrackResultEvent{packet.frameSeq, std::move(*trackResults)};
        deliverTrackResult(*trackEvent, packet);
    }
    m_bus.emit(Dss::Core::ProcessingCompleteEvent{packet.frameSeq, displayStats});
    if (trackEvent) {
        m_bus.emit(*trackEvent);
    }

    m_bus.emit(Dss::Core::RotatedFrameReadyEvent{packet.frameSeq});
    if ((rawImage && !rawImage->empty()) || (displayImage && !displayImage->empty())) {
        Dss::Core::ImageReadyForSendEvent::ImageFactory imageFactory;
        if (rawImage && !rawImage->empty()) {
            imageFactory = [rawImage, displayWindow] {
                return std::make_shared<const std::vector<std::uint8_t>>(
                    stretchDisplayImage(*rawImage, displayWindow));
            };
        }
        m_bus.emit(Dss::Core::ImageReadyForSendEvent{
            .frameSeq = packet.frameSeq,
            .width = packet.width,
            .height = packet.height,
            .image = displayImage,
            .imageFactory = std::move(imageFactory),
            .retainedSourceBytes = rawImage ? rawImage->capacity() * sizeof(std::uint16_t) : 0,
        });
    }
}

void ImageProcessor::workerLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        auto packetOpt = m_frameChannel.pop(token);
        if (!packetOpt) {
            break;
        }

        auto& packet = *packetOpt;
        const auto started = std::chrono::steady_clock::now();
        {
            std::scoped_lock lock(m_resourceMutex);
            m_resources.activeItems = 1;
            m_resources.activeBytes = imagePayloadBytes(packet);
        }
        processFrame(packet);
        {
            std::scoped_lock lock(m_resourceMutex);
            m_resources.activeItems = 0;
            m_resources.activeBytes = 0;
            ++m_resources.completedItems;
            m_resources.lastWorkMicroseconds = Dss::Core::elapsedMicroseconds(started);
            m_resources.maxWorkMicroseconds =
                (std::max)(m_resources.maxWorkMicroseconds, m_resources.lastWorkMicroseconds);
        }
    }
    m_running.store(false);
}

}  // namespace Dss::Processing
