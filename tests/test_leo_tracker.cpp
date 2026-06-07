#include <gtest/gtest.h>

#include "dss/tracking/leo_tracker.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.spdLowAe = 0.01F;
    settings.thresholdAe = 0.001F;
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

auto makeEmptyFrame(uint64_t frameSeq) -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements measurements{};
    measurements.frameSeq = frameSeq;
    measurements.frameFreq = 1.0F;
    return measurements;
}

}  // namespace

TEST(LeoTracker, FindsThreeFrameTargetWithConsistentAeMotion) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());

    const auto targets = tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F)));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_EQ(target.targetId, "leo-1");
    EXPECT_TRUE(target.living);
    EXPECT_FLOAT_EQ(target.validity, 1.0F);

    ASSERT_EQ(target.frameInfos.size(), 3U);
    EXPECT_EQ(target.frameInfos[0].frameSeq, 1U);
    EXPECT_EQ(target.frameInfos[1].frameSeq, 2U);
    EXPECT_EQ(target.frameInfos[2].frameSeq, 3U);
    EXPECT_TRUE(target.frameInfos[0].valid);
    EXPECT_TRUE(target.frameInfos[1].valid);
    EXPECT_TRUE(target.frameInfos[2].valid);

    EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 10.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 8.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 130.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 224.0F);
    EXPECT_NEAR(target.predictedSpdAe.x, 0.03F, 1.0e-6F);
    EXPECT_NEAR(target.predictedSpdAe.y, 0.02F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.09F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.06F, 1.0e-6F);
}

TEST(LeoTracker, RejectsThreeFrameTargetWhenAeMotionBelowLowThreshold) {
    auto settings = makeSettings();
    settings.spdLowAe = 0.05F;
    Dss::Tracking::LeoTracker tracker(settings);

    EXPECT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());

    EXPECT_TRUE(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).empty());
}

TEST(LeoTracker, VerifiesCandidateOnFourthFrameNearPredictedAePosition) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    ASSERT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    ASSERT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).size(), 1U);

    const auto targets = tracker.track(makeFrame(4, makeBlob(130.0F, 224.0F, 1.09F, 2.06F)));

    ASSERT_EQ(targets.size(), 1U);
    const auto& target = targets.front();
    EXPECT_EQ(target.targetId, "leo-1");
    EXPECT_TRUE(target.living);
    EXPECT_FLOAT_EQ(target.validity, 1.0F);
    ASSERT_EQ(target.frameInfos.size(), 4U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 4U);
    EXPECT_TRUE(target.frameInfos.back().valid);

    EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 10.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 8.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 140.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 232.0F);
    EXPECT_NEAR(target.predictedSpdAe.x, 0.03F, 1.0e-6F);
    EXPECT_NEAR(target.predictedSpdAe.y, 0.02F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.12F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.08F, 1.0e-6F);
}

TEST(LeoTracker, ContinuesTrackingVerifiedTargetOnFifthFrame) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    ASSERT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    ASSERT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(4, makeBlob(130.0F, 224.0F, 1.09F, 2.06F))).size(), 1U);

    const auto tracked = tracker.track(makeFrame(5, makeBlob(140.0F, 232.0F, 1.12F, 2.08F)));

    ASSERT_EQ(tracked.size(), 1U);
    const auto& target = tracked.front();
    EXPECT_EQ(target.targetId, "leo-1");
    EXPECT_TRUE(target.living);
    EXPECT_FLOAT_EQ(target.validity, 1.0F);
    ASSERT_EQ(target.frameInfos.size(), 5U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 5U);
    EXPECT_TRUE(target.frameInfos.back().valid);

    EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 10.0F);
    EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 8.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 150.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 240.0F);
    EXPECT_NEAR(target.predictedSpdAe.x, 0.03F, 1.0e-6F);
    EXPECT_NEAR(target.predictedSpdAe.y, 0.02F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.15F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.10F, 1.0e-6F);
}

TEST(LeoTracker, KeepsVerifiedTargetLivingAfterSingleTrackMiss) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    ASSERT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    ASSERT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(4, makeBlob(130.0F, 224.0F, 1.09F, 2.06F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(5, makeBlob(140.0F, 232.0F, 1.12F, 2.08F))).size(), 1U);

    const auto tracked = tracker.track(makeEmptyFrame(6));

    ASSERT_EQ(tracked.size(), 1U);
    const auto& target = tracked.front();
    EXPECT_EQ(target.targetId, "leo-1");
    EXPECT_TRUE(target.living);
    EXPECT_NEAR(target.validity, 5.0F / 6.0F, 1.0e-6F);
    ASSERT_EQ(target.frameInfos.size(), 6U);
    const auto& latest = target.frameInfos.back();
    EXPECT_EQ(latest.frameSeq, 6U);
    EXPECT_FALSE(latest.valid);
    EXPECT_FLOAT_EQ(latest.measuredBlob.centroid.x, 150.0F);
    EXPECT_FLOAT_EQ(latest.measuredBlob.centroid.y, 240.0F);
    EXPECT_NEAR(latest.measuredBlob.posAe.x, 1.15F, 1.0e-6F);
    EXPECT_NEAR(latest.measuredBlob.posAe.y, 2.10F, 1.0e-6F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 160.0F);
    EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 248.0F);
    EXPECT_NEAR(target.predictedPosAe.x, 1.18F, 1.0e-6F);
    EXPECT_NEAR(target.predictedPosAe.y, 2.12F, 1.0e-6F);
}

TEST(LeoTracker, DropsUnverifiedCandidateAndAllowsRediscoveryAfterFourthFrameMiss) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    ASSERT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    ASSERT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).size(), 1U);

    EXPECT_TRUE(tracker.track(makeFrame(4, makeBlob(400.0F, 500.0F, 9.00F, 9.00F))).empty());

    EXPECT_TRUE(tracker.track(makeFrame(5, makeBlob(200.0F, 300.0F, 3.00F, 4.00F))).empty());
    EXPECT_TRUE(tracker.track(makeFrame(6, makeBlob(210.0F, 308.0F, 3.03F, 4.02F))).empty());

    const auto rediscovered = tracker.track(makeFrame(7, makeBlob(220.0F, 316.0F, 3.06F, 4.04F)));

    ASSERT_EQ(rediscovered.size(), 1U);
    const auto& target = rediscovered.front();
    EXPECT_EQ(target.targetId, "leo-1");
    EXPECT_TRUE(target.living);
    ASSERT_EQ(target.frameInfos.size(), 3U);
    EXPECT_EQ(target.frameInfos.front().frameSeq, 5U);
    EXPECT_EQ(target.frameInfos.back().frameSeq, 7U);
}
