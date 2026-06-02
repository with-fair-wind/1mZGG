#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "dss/core/event_bus.h"
#include "dss/processing/bounded_channel.h"
#include "dss/processing/frame_packet.h"
#include "dss/processing/i_processing_strategy.h"
#include "dss/processing/processing_pipeline.h"
#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Processing {

class ImageProcessor {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit ImageProcessor(MessageBus& bus);
    ~ImageProcessor();

    void start();
    void stop();

    [[nodiscard]] bool submitFrame(FramePacket packet);
    [[nodiscard]] auto droppedFrames() const -> uint64_t;

    void setProcessingStrategy(std::unique_ptr<IProcessingStrategy> strategy);
    void setTrackingStrategy(std::unique_ptr<Dss::Tracking::ITrackingStrategy> strategy);

    [[nodiscard]] auto currentProcessingMode() const -> Dss::Core::ProcessingMode;
    [[nodiscard]] auto currentTrackMode() const -> Dss::Core::TrackMode;

private:
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;
    BoundedChannel<FramePacket, 4> m_frameChannel;
    std::jthread m_workerThread;
    std::atomic<uint64_t> m_droppedFrames{0};

    mutable std::mutex m_strategyMutex;
    ProcessingPipeline m_pipeline;
    std::unique_ptr<Dss::Tracking::ITrackingStrategy> m_trackStrategy;
};

}  // namespace Dss::Processing
