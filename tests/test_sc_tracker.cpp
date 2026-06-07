#include <gtest/gtest.h>

#include "dss/tracking/sc_tracker.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.searchRadius = 50.0F;
    settings.thresholdMeo = 0.5F;
    settings.opticParams.fovCenterX = 3072.0F;
    settings.opticParams.fovCenterY = 3072.0F;
    return settings;
}

auto makeBlob(float x, float y, float azimuth, float elevation) -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.centroid = Dss::Core::Vec2f{x, y};
    blob.minX = x - 1.0F;
    blob.maxX = x + 1.0F;
    blob.minY = y - 1.0F;
    blob.maxY = y + 1.0F;
    blob.area = 4.0F;
    blob.dn = 100.0F;
    blob.posAe = Dss::Core::Vec2f{azimuth, elevation};
    return blob;
}

auto makeFrame(uint64_t frameSeq, Dss::Core::MeasuredBlob blob) -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements measurements{};
    measurements.frameSeq = frameSeq;
    measurements.frameFreq = 1.0F;
    measurements.targetBlobs.push_back(blob);
    return measurements;
}

}  // namespace

TEST(ScTracker, FindsThreeFrameTargetWithConsistentFrameMotion) {
    Dss::Tracking::ScTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(3060.0F, 3060.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(3064.0F, 3063.0F, 1.01F, 2.01F))).empty());

    const auto targets = tracker.track(makeFrame(3, makeBlob(3068.0F, 3066.0F, 1.02F, 2.02F)));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_EQ(target.targetId, "sc-1");
    EXPECT_TRUE(target.living);
    EXPECT_FLOAT_EQ(target.validity, 1.0F);

    ASSERT_EQ(target.frameInfos.size(), 3U);
    EXPECT_EQ(target.frameInfos[0].frameSeq, 1U);
    EXPECT_EQ(target.frameInfos[1].frameSeq, 2U);
    EXPECT_EQ(target.frameInfos[2].frameSeq, 3U);
    EXPECT_TRUE(target.frameInfos[0].valid);
    EXPECT_TRUE(target.frameInfos[1].valid);
    EXPECT_TRUE(target.frameInfos[2].valid);

    EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 4.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 3.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 3072.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 3069.0F);
    EXPECT_NEAR(target.predictedSpdAe.x, 0.01F, 1.0e-6F);
    EXPECT_NEAR(target.predictedSpdAe.y, 0.01F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.03F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.03F, 1.0e-6F);
}
