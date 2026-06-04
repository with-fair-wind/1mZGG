#include <vector>

#include <gtest/gtest.h>

#include "dss/tracking/geo_tracker.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.opticParams.imageWidth = 1000;
    settings.opticParams.imageHeight = 1000;
    settings.opticParams.fovCenterX = 500.0F;
    settings.opticParams.fovCenterY = 500.0F;
    settings.opticParams.pixelScale = 0.01F;
    settings.searchRadius = 20.0F;
    settings.thresholdGazeMode = 2.5F;
    settings.thresholdStarMode = 10.0F;
    return settings;
}

auto makeBlob(float x, float y, float aeX, float aeY) -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.centroid = Dss::Core::Vec2f{x, y};
    blob.minX = x - 1.0F;
    blob.maxX = x + 1.0F;
    blob.minY = y - 1.0F;
    blob.maxY = y + 1.0F;
    blob.area = 16.0F;
    blob.dn = 1000.0F;
    blob.posAe = Dss::Core::Vec2f{aeX, aeY};
    blob.alpha = static_cast<double>(aeX) * 0.01;
    blob.sigma = static_cast<double>(aeY) * 0.01;
    return blob;
}

auto makeFrame(uint64_t seq, int second) -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements frame{};
    frame.frameSeq = seq;
    frame.timestamp.year = 2026;
    frame.timestamp.month = 6;
    frame.timestamp.day = 4;
    frame.timestamp.hour = 12;
    frame.timestamp.minute = 0;
    frame.timestamp.second = second;
    frame.frameFreq = 1.0F;
    return frame;
}

void addCenteredStars(Dss::Core::FrameMeasurements& frame, float dx, float dy) {
    const std::vector<Dss::Core::Vec2f> bases{
        {420.25F, 420.75F}, {450.25F, 450.75F}, {480.25F, 480.75F},
        {510.25F, 510.75F}, {540.25F, 540.75F},
    };
    for (const auto& base : bases) {
        frame.starBlobs.push_back(makeBlob(base.x + dx, base.y + dy, 0.0F, 0.0F));
    }
}

void addGeoTarget(Dss::Core::FrameMeasurements& frame, float x, float y) {
    const auto aeX = (x - 100.0F) * 0.01F;
    const auto aeY = (y - 100.0F) * 0.01F;
    frame.targetBlobs.push_back(makeBlob(x, y, aeX, aeY));
}

}  // namespace

TEST(GeoTracker, EstimatesStarSpeedFromDominantCenteredMotion) {
    const auto settings = makeSettings();
    auto first = makeFrame(1, 0);
    first.fovCenterAe = Dss::Core::Vec2f{10.0F, 20.0F};
    addCenteredStars(first, 0.4F, 0.8F);

    auto second = makeFrame(2, 0);
    second.timestamp.millisecond = 200;
    second.fovCenterAe = Dss::Core::Vec2f{10.1F, 19.9F};
    addCenteredStars(second, 3.8F, -1.2F);

    const std::vector<Dss::Core::FrameMeasurements> fifo{first, second};

    const auto result = Dss::Tracking::estimateGeoStarSpeed(
        fifo, settings.ratioFov, settings.searchRadius, settings.opticParams);

    ASSERT_EQ(result.status, Dss::Tracking::GeoStarSpeedStatus::Ok);
    EXPECT_EQ(result.matchCount, 5);
    EXPECT_FLOAT_EQ(result.frameSpeed.x, 3.0F);
    EXPECT_FLOAT_EQ(result.frameSpeed.y, -2.0F);
    EXPECT_NEAR(result.aeSpeed.x, 0.65F, 1.0e-5F);
    EXPECT_NEAR(result.aeSpeed.y, -0.6F, 1.0e-5F);
}

TEST(GeoTracker, FindsFourFrameTargetThatDoesNotMoveLikeStars) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 4; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);

        const auto targets = tracker.track(frame);
        if (index < 3) {
            EXPECT_TRUE(targets.empty());
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        EXPECT_TRUE(target.living);
        ASSERT_EQ(target.frameInfos.size(), 4U);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 5.0F);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 4.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 125.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 116.0F);
    }
}

TEST(GeoTracker, ContinuesTrackedTargetOnNextFrame) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        EXPECT_TRUE(target.living);
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_EQ(target.frameInfos.back().frameSeq, 5U);
        EXPECT_TRUE(target.frameInfos.back().valid);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 5.0F);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 4.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 130.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 120.0F);
    }
}

TEST(GeoTracker, EndsTrackedTargetAfterConfiguredConsecutiveInvalidFrames) {
    auto settings = makeSettings();
    settings.numFramesLiving = 3;
    Dss::Tracking::GeoTracker tracker(settings);

    for (uint64_t index = 0; index < 7; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 6) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_GE(target.frameInfos.size(), 7U);
        EXPECT_FALSE(target.frameInfos[target.frameInfos.size() - 1U].valid);
        EXPECT_FALSE(target.frameInfos[target.frameInfos.size() - 2U].valid);
        EXPECT_FALSE(target.frameInfos[target.frameInfos.size() - 3U].valid);
        EXPECT_FALSE(target.living);
    }
}

TEST(GeoTracker, EndsTrackedTargetWhenPredictionLeavesImageBounds) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 985.0F + static_cast<float>(index) * 5.0F,
                         120.0F + static_cast<float>(index) * 4.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_GE(target.frameInfos.size(), 5U);
        EXPECT_FALSE(target.frameInfos.back().valid);
        EXPECT_GT(target.predictedPosFrame.x, 1000.0F);
        EXPECT_FALSE(target.living);
    }
}

TEST(GeoTracker, DoesNotReuseSameMeasurementForMultipleTrackedTargets) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
            addGeoTarget(frame, 125.0F + static_cast<float>(index) * 5.0F,
                         120.0F + static_cast<float>(index) * 4.0F);
        } else {
            addGeoTarget(frame, 135.0F, 126.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 2U);
        int validLatestCount = 0;
        int invalidLatestCount = 0;
        for (const auto& target : targets) {
            ASSERT_EQ(target.frameInfos.size(), 5U);
            EXPECT_EQ(target.frameInfos.back().frameSeq, 5U);
            if (target.frameInfos.back().valid) {
                ++validLatestCount;
                EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 135.0F);
                EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 126.0F);
            } else {
                ++invalidLatestCount;
            }
        }

        EXPECT_EQ(validLatestCount, 1);
        EXPECT_EQ(invalidLatestCount, 1);
    }
}

TEST(GeoTracker, RediscoversTargetAfterTrackedTargetIsLost) {
    auto settings = makeSettings();
    settings.numFramesLiving = 3;
    Dss::Tracking::GeoTracker tracker(settings);

    for (uint64_t index = 0; index < 11; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
        } else if (index >= 7) {
            const auto newTrackIndex = static_cast<float>(index - 7U);
            addGeoTarget(frame, 300.0F + newTrackIndex * 4.0F, 350.0F + newTrackIndex * 5.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 10) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        EXPECT_TRUE(target.living);
        ASSERT_EQ(target.frameInfos.size(), 4U);
        EXPECT_EQ(target.frameInfos.front().frameSeq, 8U);
        EXPECT_EQ(target.frameInfos.back().frameSeq, 11U);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 4.0F);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 5.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 316.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 370.0F);
    }
}
