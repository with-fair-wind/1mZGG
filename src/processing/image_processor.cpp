#include "dss/processing/image_processor.h"

#include "dss/core/events.h"

namespace Dss::Processing {

ImageProcessor::ImageProcessor(MessageBus& bus) : m_bus(bus) {}

ImageProcessor::~ImageProcessor() {
    stop();
}

void ImageProcessor::start() {
    if (m_running.exchange(true)) {
        return;
    }
    m_frameChannel.open();
    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
}

void ImageProcessor::stop() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_frameChannel.close();
        m_workerThread.join();
    }
    m_running.store(false);
}

bool ImageProcessor::submitFrame(FramePacket packet) {
    if (!m_frameChannel.tryPush(std::move(packet))) {
        m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

auto ImageProcessor::droppedFrames() const -> uint64_t {
    return m_droppedFrames.load(std::memory_order_relaxed);
}

bool ImageProcessor::isRunning() const {
    return m_running.load();
}

void ImageProcessor::setProcessingStrategy(std::unique_ptr<IProcessingStrategy> strategy) {
    std::lock_guard lock(m_strategyMutex);
    m_pipeline.setBackend(std::move(strategy));
}

void ImageProcessor::setTrackingStrategy(
    std::unique_ptr<Dss::Tracking::ITrackingStrategy> strategy) {
    std::lock_guard lock(m_strategyMutex);
    m_trackStrategy = std::move(strategy);
}

auto ImageProcessor::currentProcessingMode() const -> Dss::Core::ProcessingMode {
    std::lock_guard lock(m_strategyMutex);
    return m_pipeline.currentMode();
}

auto ImageProcessor::currentTrackMode() const -> Dss::Core::TrackMode {
    std::lock_guard lock(m_strategyMutex);
    return m_trackStrategy ? m_trackStrategy->mode() : Dss::Core::TrackMode::Init;
}

void ImageProcessor::setDisplayStretchSettings(DisplayStretchSettings settings) {
    std::lock_guard lock(m_displayStretchMutex);
    m_displayStretchSettings = settings;
}

auto ImageProcessor::displayStretchSettings() const -> DisplayStretchSettings {
    return currentDisplayStretchSettings();
}

auto ImageProcessor::currentDisplayStretchSettings() const -> DisplayStretchSettings {
    std::lock_guard lock(m_displayStretchMutex);
    return m_displayStretchSettings;
}

void ImageProcessor::workerLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        auto packetOpt = m_frameChannel.pop(token);
        if (!packetOpt) {
            break;
        }

        auto& packet = *packetOpt;

        ProcessingResult procResult;
        {
            std::lock_guard lock(m_strategyMutex);
            procResult = m_pipeline.process(packet);
        }

        std::vector<Dss::Core::TargetInfo> trackResults;
        {
            std::lock_guard lock(m_strategyMutex);
            const auto canTrackDirectFrame = !m_pipeline.hasBackend();
            if (m_trackStrategy && (procResult.success || canTrackDirectFrame)) {
                Dss::Core::FrameMeasurements meas{};
                meas.frameSeq = packet.frameSeq;
                meas.timestamp = packet.metadata.timestamp;
                meas.fovCenterAe = packet.metadata.pointingAe;
                meas.exposureTime = packet.metadata.exposureTime;
                meas.frameFreq = packet.metadata.frameFrequency;
                meas.targetBlobs = procResult.success ? std::move(procResult.targetBlobs)
                                                      : std::move(packet.targetBlobs);
                meas.validatedTargetBlobs = procResult.success
                                                ? std::move(procResult.validatedTargetBlobs)
                                                : std::move(packet.validatedTargetBlobs);
                meas.starBlobs = procResult.success ? std::move(procResult.starBlobs)
                                                    : std::move(packet.starBlobs);

                trackResults = m_trackStrategy->track(meas);
            }
        }

        auto displayStats = procResult.success ? procResult.stats : packet.stats;
        std::vector<std::uint8_t> displayBuffer;
        const auto expectedPixelCount =
            static_cast<std::size_t>(packet.width) * static_cast<std::size_t>(packet.height);
        if (expectedPixelCount > 0U && packet.rawImage.size() == expectedPixelCount) {
            auto display = buildDisplayImage(packet.rawImage, currentDisplayStretchSettings());
            displayStats = display.stats;
            displayBuffer = std::move(display.displayImage);
        } else if (!procResult.displayImage.empty()) {
            displayBuffer = std::move(procResult.displayImage);
        } else {
            displayBuffer = std::move(packet.displayImage);
        }

        auto displayImage =
            std::make_shared<const std::vector<std::uint8_t>>(std::move(displayBuffer));
        std::shared_ptr<const std::vector<std::uint16_t>> rawImage;
        if (expectedPixelCount > 0U && packet.rawImage.size() == expectedPixelCount) {
            rawImage =
                std::make_shared<const std::vector<std::uint16_t>>(std::move(packet.rawImage));
        }
        m_bus.emit(Dss::Core::DisplayRefreshEvent{packet.frameSeq, packet.width, packet.height,
                                                  packet.width, std::move(displayImage),
                                                  std::move(rawImage)});
        m_bus.emit(Dss::Core::ProcessingCompleteEvent{packet.frameSeq, displayStats});

        if (!trackResults.empty()) {
            m_bus.emit(Dss::Core::TrackResultEvent{packet.frameSeq, std::move(trackResults)});
        }

        m_bus.emit(Dss::Core::RotatedFrameReadyEvent{packet.frameSeq});
        m_bus.emit(Dss::Core::ImageSendEvent{packet.frameSeq});
    }
    m_running.store(false);
}

}  // namespace Dss::Processing
