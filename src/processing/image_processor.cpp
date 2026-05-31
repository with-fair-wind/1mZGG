#include "dss/processing/image_processor.h"

#include "dss/core/events.h"

namespace Dss::Processing
{

ImageProcessor::ImageProcessor(MessageBus& bus)
    : m_bus(bus)
{
}

ImageProcessor::~ImageProcessor()
{
    stop();
}

void ImageProcessor::start()
{
    m_frameChannel.open();
    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
}

void ImageProcessor::stop()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_frameChannel.close();
        m_workerThread.join();
    }
}

bool ImageProcessor::submitFrame(FramePacket packet)
{
    if (!m_frameChannel.tryPush(std::move(packet)))
    {
        m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

auto ImageProcessor::droppedFrames() const -> uint64_t
{
    return m_droppedFrames.load(std::memory_order_relaxed);
}

void ImageProcessor::setProcessingStrategy(std::unique_ptr<IProcessingStrategy> strategy)
{
    std::lock_guard lock(m_strategyMutex);
    m_pipeline.setBackend(std::move(strategy));
}

void ImageProcessor::setTrackingStrategy(std::unique_ptr<Dss::Tracking::ITrackingStrategy> strategy)
{
    std::lock_guard lock(m_strategyMutex);
    m_trackStrategy = std::move(strategy);
}

auto ImageProcessor::currentProcessingMode() const -> Dss::Core::ProcessingMode
{
    std::lock_guard lock(m_strategyMutex);
    return m_pipeline.currentMode();
}

auto ImageProcessor::currentTrackMode() const -> Dss::Core::TrackMode
{
    std::lock_guard lock(m_strategyMutex);
    return m_trackStrategy ? m_trackStrategy->mode() : Dss::Core::TrackMode::Init;
}

void ImageProcessor::workerLoop(std::stop_token token)
{
    while (!token.stop_requested())
    {
        auto packetOpt = m_frameChannel.pop(token);
        if (!packetOpt)
        {
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
            if (m_trackStrategy && procResult.success)
            {
                Dss::Core::FrameMeasurements meas{};
                meas.frameSeq = packet.frameSeq;
                meas.timestamp = packet.metadata.timestamp;
                meas.fovCenterAe = packet.metadata.pointingAe;
                meas.exposureTime = packet.metadata.exposureTime;
                meas.frameFreq = packet.metadata.frameFrequency;
                meas.targetBlobs = std::move(procResult.targetBlobs);
                meas.starBlobs = std::move(procResult.starBlobs);

                trackResults = m_trackStrategy->track(meas);
            }
        }

        auto displayImage = std::make_shared<const std::vector<uint8_t>>(
            procResult.displayImage.empty() ? packet.displayImage : std::move(procResult.displayImage));
        m_bus.emit(Dss::Core::DisplayRefreshEvent{
            packet.frameSeq,
            packet.width,
            packet.height,
            packet.width,
            std::move(displayImage)});
        m_bus.emit(Dss::Core::ProcessingCompleteEvent{packet.frameSeq, procResult.stats});

        if (!trackResults.empty())
        {
            m_bus.emit(Dss::Core::TrackResultEvent{packet.frameSeq, std::move(trackResults)});
        }

        m_bus.emit(Dss::Core::RotatedFrameReadyEvent{packet.frameSeq});
        m_bus.emit(Dss::Core::ImageSendEvent{packet.frameSeq});
    }
}

} // namespace Dss::Processing
