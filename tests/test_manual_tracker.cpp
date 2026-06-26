#include <cmath>

#include <gtest/gtest.h>

#include "dss/core/constants.h"
#include "dss/tracking/manual_tracker.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.opticParams.fovCenterX = 3072.0F;
    settings.opticParams.fovCenterY = 3072.0F;
    settings.opticParams.pixelScale = 0.001F;
    return settings;
}

auto makeMeasurements(uint64_t frameSeq) -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements measurements{};
    measurements.frameSeq = frameSeq;
    measurements.fovCenterAe = Dss::Core::Vec2f{100.0F, 45.0F};
    measurements.frameFreq = 20.0F;
    return measurements;
}

}  // namespace

TEST(ManualTracker, DoesNotEmitTargetBeforeManualSelection) {
    Dss::Tracking::ManualTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeMeasurements(1)).empty());
}

TEST(ManualTracker, EmitsManualTargetAtSelectedPixel) {
    Dss::Tracking::ManualTracker tracker(makeSettings());
    Dss::Core::MeasuredBlob selected{};
    selected.centroid = Dss::Core::Vec2f{3082.0F, 3062.0F};

    tracker.setManualTarget(selected);

    const auto targets = tracker.track(makeMeasurements(7));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    ASSERT_EQ(target.frameInfos.size(), 1U);
    EXPECT_EQ(target.targetId, "manual");
    EXPECT_TRUE(target.living);
    EXPECT_FLOAT_EQ(target.validity, 1.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 3082.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 3062.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 0.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 0.0F);

    const auto expectedElevation = 45.01F;
    const auto expectedAzimuth =
        100.0F + 0.01F / std::cos(static_cast<double>(expectedElevation) * Dss::Core::DegToRad);
    EXPECT_NEAR(target.predictedPosAe.x, expectedAzimuth, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosAe.y, expectedElevation, 1.0e-5F);

    const auto& frame = target.frameInfos.front();
    EXPECT_EQ(frame.frameSeq, 7U);
    EXPECT_TRUE(frame.valid);
    EXPECT_FLOAT_EQ(frame.measuredBlob.minX, 3072.0F);
    EXPECT_FLOAT_EQ(frame.measuredBlob.maxX, 3092.0F);
    EXPECT_FLOAT_EQ(frame.measuredBlob.minY, 3052.0F);
    EXPECT_FLOAT_EQ(frame.measuredBlob.maxY, 3072.0F);
    EXPECT_FLOAT_EQ(frame.measuredBlob.area, 100.0F);
    EXPECT_FLOAT_EQ(frame.measuredBlob.dn, 10000.0F);
}

TEST(ManualTracker, ResetClearsManualSelectionAndTrackHistory) {
    Dss::Tracking::ManualTracker tracker(makeSettings());
    Dss::Core::MeasuredBlob selected{};
    selected.centroid = Dss::Core::Vec2f{3082.0F, 3062.0F};
    tracker.setManualTarget(selected);

    ASSERT_EQ(tracker.track(makeMeasurements(1)).size(), 1U);

    tracker.reset();

    EXPECT_TRUE(tracker.track(makeMeasurements(2)).empty());
}
