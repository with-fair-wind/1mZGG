#include <gtest/gtest.h>

#include "dss/tracking/strategy/leo_tracker.h"

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
    ASSERT_EQ(target.frameInfos.size(), 5U);
    EXPECT_EQ(target.totalFrameCount(), 6U);
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

TEST(LeoTracker, DropsTrackedTargetAfterFiveConsecutiveTrackMisses) {
    Dss::Tracking::LeoTracker tracker(makeSettings());

    ASSERT_TRUE(tracker.track(makeFrame(1, makeBlob(100.0F, 200.0F, 1.00F, 2.00F))).empty());
    ASSERT_TRUE(tracker.track(makeFrame(2, makeBlob(110.0F, 208.0F, 1.03F, 2.02F))).empty());
    ASSERT_EQ(tracker.track(makeFrame(3, makeBlob(120.0F, 216.0F, 1.06F, 2.04F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(4, makeBlob(130.0F, 224.0F, 1.09F, 2.06F))).size(), 1U);
    ASSERT_EQ(tracker.track(makeFrame(5, makeBlob(140.0F, 232.0F, 1.12F, 2.08F))).size(), 1U);

    EXPECT_EQ(tracker.track(makeEmptyFrame(6)).size(), 1U);
    EXPECT_EQ(tracker.track(makeEmptyFrame(7)).size(), 1U);
    EXPECT_EQ(tracker.track(makeEmptyFrame(8)).size(), 1U);
    EXPECT_EQ(tracker.track(makeEmptyFrame(9)).size(), 1U);
    EXPECT_TRUE(tracker.track(makeEmptyFrame(10)).empty());
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

TEST(LeoTracker, LongTrackBoundsHistoryAndRetainsFiveMissRetirement) {
    Dss::Tracking::LeoTracker tracker(makeSettings());
    std::string id;
    float expectedValidity = 1.0F;
    for (std::uint64_t seq = 1; seq <= 1000; ++seq) {
        const auto index = static_cast<float>(seq - 1);
        const bool valid = seq % 100 != 0;
        const auto targets =
            tracker.track(valid ? makeFrame(seq, makeBlob(100 + index * 0.125F, 200 + index * 0.25F,
                                                          1 + index * 0.03F, 2 + index * 0.02F))
                                : makeEmptyFrame(seq));
        if (seq < 3) {
            ASSERT_TRUE(targets.empty());
            continue;
        }
        ASSERT_EQ(targets.size(), 1U) << "seq=" << seq;
        const auto& target = targets.front();
        if (seq == 3) {
            id = target.targetId;
        } else {
            expectedValidity =
                (static_cast<float>(seq - 1) * expectedValidity + (valid ? 1.0F : 0.0F)) /
                static_cast<float>(seq);
        }
        ASSERT_TRUE(target.living);
        ASSERT_EQ(target.targetId, id);
        ASSERT_EQ(target.totalFrameCount(), seq);
        ASSERT_LE(target.frameInfos.size(), 5U);
        ASSERT_FLOAT_EQ(target.validity, expectedValidity);
        ASSERT_EQ(target.frameInfos.back().valid, valid);
    }
    // Frame 1000 was already a miss; the fourth additional miss retires the target.
    for (std::uint64_t seq = 1001; seq <= 1004; ++seq) {
        EXPECT_EQ(tracker.track(makeEmptyFrame(seq)).empty(), seq == 1004);
    }
    tracker.reset();
    for (std::uint64_t seq = 1; seq <= 4; ++seq) {
        const auto index = static_cast<float>(seq - 1);
        const auto targets = tracker.track(makeFrame(
            seq, makeBlob(100 + index, 200 + index, 1 + index * 0.03F, 2 + index * 0.02F)));
        if (seq == 4) {
            ASSERT_EQ(targets.size(), 1U);
            EXPECT_EQ(targets.front().totalFrameCount(), 4U);
            EXPECT_EQ(targets.front().discardedFrameCount, 0U);
        }
    }
}
