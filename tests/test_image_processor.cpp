#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/tracking/strategy/i_tracking_strategy.h"
#include "dss/tracking/strategy/manual_tracker.h"

using namespace std::chrono_literals;

namespace {

class DisplayStrategy final : public Dss::Processing::IProcessingStrategy {
public:
    [[nodiscard]] auto process(const Dss::Processing::FramePacket& input)
        -> Dss::Processing::ProcessingResult override {
        Dss::Processing::ProcessingResult result;
        result.success = true;
        result.stats.maxVal = static_cast<double>(input.frameSeq);
        result.displayImage = {1, 2, 3, 4};
        return result;
    }

    [[nodiscard]] auto name() const -> std::string_view override {
        return "display";
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }
};

class ThrowingStrategy final : public Dss::Processing::IProcessingStrategy {
public:
    [[nodiscard]] auto process(const Dss::Processing::FramePacket&)
        -> Dss::Processing::ProcessingResult override {
        throw std::runtime_error("processing failed");
    }

    [[nodiscard]] auto name() const -> std::string_view override {
        return "throwing";
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }
};

class BlockingDisplayStrategy final : public Dss::Processing::IProcessingStrategy {
public:
    BlockingDisplayStrategy(std::latch& entered, std::latch& release)
        : m_entered(entered), m_release(release) {}

    [[nodiscard]] auto process(const Dss::Processing::FramePacket& input)
        -> Dss::Processing::ProcessingResult override {
        if (!m_blocked.exchange(true)) {
            m_entered.count_down();
            m_release.wait();
        }
        DisplayStrategy delegate;
        return delegate.process(input);
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }

    [[nodiscard]] auto name() const -> std::string_view override {
        return "blocking-display";
    }

private:
    std::latch& m_entered;
    std::latch& m_release;
    std::atomic<bool> m_blocked{false};
};

class CapturingTrackStrategy final : public Dss::Tracking::ITrackingStrategy {
public:
    explicit CapturingTrackStrategy(std::promise<Dss::Core::FrameMeasurements>& promise)
        : m_promise(promise) {}

    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Geo;
    }

    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override {
        m_promise.set_value(measurements);
        return {};
    }

    void reset() override {}

private:
    std::promise<Dss::Core::FrameMeasurements>& m_promise;
};

}  // namespace

TEST(ImageProcessor, PublishesDisplayFramePayload) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    processor.setProcessingStrategy(std::make_unique<DisplayStrategy>());

    std::promise<Dss::Core::DisplayRefreshEvent> displayPromise;
    auto displayFuture = displayPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [&displayPromise](const Dss::Core::DisplayRefreshEvent& event) {
            displayPromise.set_value(event);
        });
    std::promise<Dss::Core::ImageReadyForSendEvent> sendPromise;
    auto sendFuture = sendPromise.get_future();
    auto sendConnection = bus.subscribe<Dss::Core::ImageReadyForSendEvent>(
        [&sendPromise](const auto& event) { sendPromise.set_value(event); });

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 42;
    packet.width = 2;
    packet.height = 2;

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(displayFuture.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(sendFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto event = displayFuture.get();
    ASSERT_TRUE(event.displayImage);
    EXPECT_EQ(event.frameSeq, 42U);
    EXPECT_EQ(event.width, 2U);
    EXPECT_EQ(event.height, 2U);
    EXPECT_EQ(*event.displayImage, (std::vector<uint8_t>{1, 2, 3, 4}));

    const auto sendEvent = sendFuture.get();
    EXPECT_EQ(sendEvent.frameSeq, 42U);
    EXPECT_EQ(sendEvent.width, 2U);
    EXPECT_EQ(sendEvent.height, 2U);
    ASSERT_TRUE(sendEvent.image);
    EXPECT_EQ(*sendEvent.image, (std::vector<uint8_t>{1, 2, 3, 4}));
}

TEST(ImageProcessor, RejectsFramesWhileStopped) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);

    Dss::Processing::FramePacket beforeStart;
    beforeStart.frameSeq = 1U;
    EXPECT_FALSE(processor.submitFrame(std::move(beforeStart)));
    processor.start();
    processor.stop();
    Dss::Processing::FramePacket afterStop;
    afterStop.frameSeq = 2U;
    EXPECT_FALSE(processor.submitFrame(std::move(afterStop)));
}

TEST(ImageProcessor, ClearsQueuedFramesBeforeRestart) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    std::latch entered{1};
    std::latch release{1};
    processor.setProcessingStrategy(std::make_unique<BlockingDisplayStrategy>(entered, release));

    std::mutex completedMutex;
    std::vector<uint64_t> completedFrames;
    auto connection = bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [&completedMutex, &completedFrames](const auto& event) {
            std::lock_guard lock(completedMutex);
            completedFrames.push_back(event.frameSeq);
        });

    processor.start();
    Dss::Processing::FramePacket first;
    first.frameSeq = 1U;
    Dss::Processing::FramePacket queued;
    queued.frameSeq = 2U;
    ASSERT_TRUE(processor.submitFrame(std::move(first)));
    ASSERT_TRUE(processor.submitFrame(std::move(queued)));
    entered.wait();

    std::jthread stopper([&processor] { processor.stop(); });
    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (processor.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const auto stoppedAccepting = !processor.isRunning();
    release.count_down();
    stopper.join();
    ASSERT_TRUE(stoppedAccepting);

    processor.start();
    Dss::Processing::FramePacket restarted;
    restarted.frameSeq = 3U;
    ASSERT_TRUE(processor.submitFrame(std::move(restarted)));

    deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard lock(completedMutex);
            if (std::find(completedFrames.begin(), completedFrames.end(), 3U) !=
                completedFrames.end()) {
                break;
            }
        }
        std::this_thread::yield();
    }
    processor.stop();

    std::lock_guard lock(completedMutex);
    EXPECT_EQ(std::count(completedFrames.begin(), completedFrames.end(), 2U), 0);
    EXPECT_EQ(std::count(completedFrames.begin(), completedFrames.end(), 3U), 1);
}

TEST(ImageProcessor, AppliesManualDisplayStretchToRawFrames) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    Dss::Processing::DisplayStretchSettings settings{};
    settings.mode = Dss::Processing::DisplayStretchMode::Manual;
    settings.low = 1000;
    settings.high = 5000;
    processor.setDisplayStretchSettings(settings);

    std::promise<Dss::Core::DisplayRefreshEvent> displayPromise;
    auto displayFuture = displayPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [&displayPromise](const Dss::Core::DisplayRefreshEvent& event) {
            displayPromise.set_value(event);
        });

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 7;
    packet.width = 2;
    packet.height = 2;
    packet.rawImage = Dss::Processing::makeSharedRawImage({500, 1000, 3000, 5000});
    packet.displayImage = {9, 9, 9, 9};

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(displayFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto event = displayFuture.get();
    ASSERT_TRUE(event.displayImage);
    EXPECT_EQ(*event.displayImage, (std::vector<uint8_t>{0, 0, 127, 255}));
    EXPECT_TRUE(event.displayStretchWindowValid);
    EXPECT_EQ(event.displayStretchLow, 1000U);
    EXPECT_EQ(event.displayStretchHigh, 5000U);
}

TEST(ImageProcessor, ConvertsUnhandledWorkerExceptionToDiagnosticEvent) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    processor.setProcessingStrategy(std::make_unique<ThrowingStrategy>());
    std::promise<Dss::Core::BackgroundTaskErrorEvent> errorPromise;
    auto errorFuture = errorPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::BackgroundTaskErrorEvent>(
        [&errorPromise](const auto& event) { errorPromise.set_value(event); });

    processor.start();
    ASSERT_TRUE(processor.submitFrame({}));

    ASSERT_EQ(errorFuture.wait_for(2s), std::future_status::ready);
    processor.stop();
    const auto error = errorFuture.get();
    EXPECT_EQ(error.component, "image_processor");
    EXPECT_EQ(error.message, "processing failed");
    EXPECT_FALSE(processor.isRunning());
}

TEST(ImageProcessor, ReusesRawBufferAndDefersCpuDisplayImage) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    processor.setCpuDisplayImageRequired(false);
    Dss::Processing::DisplayStretchSettings settings{};
    settings.mode = Dss::Processing::DisplayStretchMode::Manual;
    settings.low = 1000;
    settings.high = 5000;
    processor.setDisplayStretchSettings(settings);

    std::promise<Dss::Core::DisplayRefreshEvent> displayPromise;
    auto displayFuture = displayPromise.get_future();
    auto displayConnection = bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [&displayPromise](const auto& event) { displayPromise.set_value(event); });
    std::promise<Dss::Core::ImageReadyForSendEvent> sendPromise;
    auto sendFuture = sendPromise.get_future();
    auto sendConnection = bus.subscribe<Dss::Core::ImageReadyForSendEvent>(
        [&sendPromise](const auto& event) { sendPromise.set_value(event); });

    auto rawImage = Dss::Processing::makeSharedRawImage({500, 1000, 3000, 5000});
    Dss::Processing::FramePacket packet;
    packet.frameSeq = 8;
    packet.width = 2;
    packet.height = 2;
    packet.rawImage = rawImage;

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));
    ASSERT_EQ(displayFuture.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(sendFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto displayEvent = displayFuture.get();
    EXPECT_EQ(displayEvent.rawImage, rawImage);
    ASSERT_TRUE(displayEvent.displayImage);
    EXPECT_TRUE(displayEvent.displayImage->empty());
    EXPECT_TRUE(displayEvent.displayStretchWindowValid);

    const auto sendEvent = sendFuture.get();
    EXPECT_EQ(sendEvent.frameSeq, 8U);
    ASSERT_TRUE(sendEvent.imageFactory);
    const auto generatedImage = sendEvent.imageFactory();
    ASSERT_TRUE(generatedImage);
    EXPECT_EQ(*generatedImage, (std::vector<uint8_t>{0, 0, 127, 255}));
}

TEST(ImageProcessor, PublishesManualTrackResultsWithoutProcessingBackend) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);

    Dss::Core::TrackingSettings settings{};
    settings.opticParams.fovCenterX = 1.0F;
    settings.opticParams.fovCenterY = 1.0F;
    settings.opticParams.pixelScale = 0.01F;

    auto tracker = std::make_unique<Dss::Tracking::ManualTracker>(settings);
    Dss::Core::MeasuredBlob selected{};
    selected.centroid = Dss::Core::Vec2f{2.0F, 0.0F};
    tracker->setManualTarget(selected);
    processor.setTrackingStrategy(std::move(tracker));

    std::promise<Dss::Core::TrackResultEvent> trackPromise;
    auto trackFuture = trackPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::TrackResultEvent>(
        [&trackPromise](Dss::Core::TrackResultEvent event) {
            trackPromise.set_value(std::move(event));
        });

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 9;
    packet.width = 2;
    packet.height = 2;
    packet.displayImage = {0, 1, 2, 3};
    packet.metadata.pointingAe = Dss::Core::Vec2f{10.0F, 20.0F};
    packet.metadata.frameFrequency = 25.0F;

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(trackFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto event = trackFuture.get();
    ASSERT_EQ(event.targets.size(), 1U);
    EXPECT_EQ(event.frameSeq, 9U);
    EXPECT_EQ(event.targets.front().targetId, "manual");
    EXPECT_TRUE(event.targets.front().living);
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.x, 2.0F);
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.y, 0.0F);
}

TEST(ImageProcessor, PassesValidatedTargetBlobsToTrackingWithoutProcessingBackend) {
    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);

    std::promise<Dss::Core::FrameMeasurements> measurementsPromise;
    auto measurementsFuture = measurementsPromise.get_future();
    processor.setTrackingStrategy(std::make_unique<CapturingTrackStrategy>(measurementsPromise));

    Dss::Core::MeasuredBlob validatedBlob{};
    validatedBlob.id = "geo";
    validatedBlob.centroid = Dss::Core::Vec2f{131.0F, 122.0F};

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 10;
    packet.displayImage = {0, 1, 2, 3};
    packet.validatedTargetBlobs.push_back(validatedBlob);

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(measurementsFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto measurements = measurementsFuture.get();
    ASSERT_EQ(measurements.validatedTargetBlobs.size(), 1U);
    EXPECT_EQ(measurements.validatedTargetBlobs.front().id, "geo");
    EXPECT_FLOAT_EQ(measurements.validatedTargetBlobs.front().centroid.x, 131.0F);
    EXPECT_FLOAT_EQ(measurements.validatedTargetBlobs.front().centroid.y, 122.0F);
}
