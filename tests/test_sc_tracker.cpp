#include <utility>
#include <vector>

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

auto makeFrame(uint64_t frameSeq, std::vector<Dss::Core::MeasuredBlob> blobs)
    -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements measurements{};
    measurements.frameSeq = frameSeq;
    measurements.frameFreq = 1.0F;
    measurements.targetBlobs = std::move(blobs);
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

TEST(ScTracker, CompressesThreeFrameCandidatesThatReuseInitialMeasurements) {
    Dss::Tracking::ScTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(3060.0F, 3060.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker
                    .track(makeFrame(2, {makeBlob(3064.0F, 3063.0F, 1.01F, 2.01F),
                                         makeBlob(3065.0F, 3063.0F, 1.02F, 2.01F)}))
                    .empty());

    const auto targets = tracker.track(makeFrame(
        3, {makeBlob(3068.0F, 3066.0F, 1.02F, 2.02F), makeBlob(3070.0F, 3066.0F, 1.04F, 2.02F)}));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    ASSERT_EQ(target.frameInfos.size(), 3U);
    EXPECT_FLOAT_EQ(target.frameInfos[1].measuredBlob.centroid.x, 3064.0F);
    EXPECT_FLOAT_EQ(target.frameInfos[2].measuredBlob.centroid.x, 3068.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 3072.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 3069.0F);
}

TEST(ScTracker, VerifiesTargetOnFourthFrameUsingPredictedFramePosition) {
    Dss::Tracking::ScTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(3060.0F, 3060.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(3064.0F, 3063.0F, 1.01F, 2.01F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(3068.0F, 3066.0F, 1.02F, 2.02F))).size(), 1U);

    auto matchingBlob = makeBlob(3072.1F, 3069.2F, 1.03F, 2.03F);
    matchingBlob.area = 9.0F;
    const auto targets = tracker.track(makeFrame(4, matchingBlob));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_TRUE(target.living);
    ASSERT_EQ(target.frameInfos.size(), 4U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 4U);
    EXPECT_TRUE(target.frameInfos.back().valid);
    EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.area, 9.0F);
    EXPECT_NEAR(target.predictedSpdFrame.x, 4.0F, 1.0e-5F);
    EXPECT_NEAR(target.predictedSpdFrame.y, 3.0F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosFrame.x, 3076.1F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosFrame.y, 3072.2F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.04F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.04F, 1.0e-6F);
}

TEST(ScTracker, ContinuesVerifiedTargetOnNextFrame) {
    Dss::Tracking::ScTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(3060.0F, 3060.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(3064.0F, 3063.0F, 1.01F, 2.01F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(3068.0F, 3066.0F, 1.02F, 2.02F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(4, makeBlob(3072.0F, 3069.0F, 1.03F, 2.03F))).size(), 1U);

    auto matchingBlob = makeBlob(3076.0F, 3072.0F, 1.04F, 2.04F);
    matchingBlob.area = 12.0F;
    const auto targets = tracker.track(makeFrame(5, matchingBlob));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_TRUE(target.living);
    ASSERT_EQ(target.frameInfos.size(), 5U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 5U);
    EXPECT_TRUE(target.frameInfos.back().valid);
    EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.area, 12.0F);
    EXPECT_NEAR(target.predictedPosFrame.x, 3080.0F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosFrame.y, 3075.0F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.05F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.05F, 1.0e-6F);
}

TEST(ScTracker, DropsTrackedTargetAfterMissAndCanRediscover) {
    Dss::Tracking::ScTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(3060.0F, 3060.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(3064.0F, 3063.0F, 1.01F, 2.01F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(3068.0F, 3066.0F, 1.02F, 2.02F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(4, makeBlob(3072.0F, 3069.0F, 1.03F, 2.03F))).size(), 1U);

    EXPECT_TRUE(tracker.track(makeFrame(5, makeBlob(3200.0F, 3200.0F, 3.00F, 4.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(6, makeBlob(3060.0F, 3060.0F, 5.00F, 6.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(7, makeBlob(3062.0F, 3062.0F, 5.01F, 6.01F))).empty());

    const auto targets = tracker.track(makeFrame(8, makeBlob(3064.0F, 3064.0F, 5.02F, 6.02F)));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_TRUE(target.living);
    ASSERT_EQ(target.frameInfos.size(), 3U);
    EXPECT_EQ(target.frameInfos.front().frameSeq, 6U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 8U);
    EXPECT_NEAR(target.predictedPosFrame.x, 3066.0F, 1.0e-5F);
    EXPECT_NEAR(target.predictedPosFrame.y, 3066.0F, 1.0e-5F);
}
