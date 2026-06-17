#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/processing/image_processor.h"
#include "dss/tracking/i_tracking_strategy.h"
#include "dss/tracking/manual_tracker.h"

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

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 42;
    packet.width = 2;
    packet.height = 2;

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(displayFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto event = displayFuture.get();
    ASSERT_TRUE(event.displayImage);
    EXPECT_EQ(event.frameSeq, 42U);
    EXPECT_EQ(event.width, 2U);
    EXPECT_EQ(event.height, 2U);
    EXPECT_EQ(*event.displayImage, (std::vector<uint8_t>{1, 2, 3, 4}));
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
    packet.rawImage = {500, 1000, 3000, 5000};
    packet.displayImage = {9, 9, 9, 9};

    processor.start();
    ASSERT_TRUE(processor.submitFrame(std::move(packet)));

    ASSERT_EQ(displayFuture.wait_for(2s), std::future_status::ready);
    processor.stop();

    const auto event = displayFuture.get();
    ASSERT_TRUE(event.displayImage);
    EXPECT_EQ(*event.displayImage, (std::vector<uint8_t>{0, 0, 127, 255}));
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
