#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/constants.h"
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

void addGeoTargetWithEquatorial(Dss::Core::FrameMeasurements& frame, float x, float y, double alpha,
                                double sigma) {
    addGeoTarget(frame, x, y);
    frame.targetBlobs.back().alpha = alpha;
    frame.targetBlobs.back().sigma = sigma;
}

void addGeoTargetWithRaDec(Dss::Core::FrameMeasurements& frame, float x, float y, double ra,
                           double dec) {
    addGeoTarget(frame, x, y);
    frame.targetBlobs.back().ra = ra;
    frame.targetBlobs.back().dec = dec;
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

TEST(GeoTracker, AssignsStableUniqueIdsToAssociatedTargets) {
    Dss::Tracking::GeoTracker tracker(makeSettings());
    std::vector<std::string> initialIds;

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);
        addGeoTarget(frame, 300.0F + static_cast<float>(index) * 3.0F,
                     360.0F + static_cast<float>(index) * 5.0F);

        const auto targets = tracker.track(frame);
        if (index < 3) {
            continue;
        }

        ASSERT_EQ(targets.size(), 2U);
        if (index == 3) {
            for (const auto& target : targets) {
                ASSERT_FALSE(target.targetId.empty());
                EXPECT_TRUE(target.targetId.starts_with("geo-"));
                initialIds.push_back(target.targetId);
            }
            ASSERT_EQ(initialIds.size(), 2U);
            EXPECT_NE(initialIds[0], initialIds[1]);
            continue;
        }

        for (const auto& target : targets) {
            EXPECT_NE(std::ranges::find(initialIds, target.targetId), initialIds.end());
        }
    }
}

TEST(GeoTracker, ExpandsTrackingSearchAfterRecentInvalidFrames) {
    auto settings = makeSettings();
    settings.searchRadius = 5.0F;
    settings.numFramesLiving = 4;
    Dss::Tracking::GeoTracker tracker(settings);

    for (uint64_t index = 0; index < 6; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, 0.0F, 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 4.0F,
                         100.0F + static_cast<float>(index) * 3.0F);
        } else if (index == 5) {
            addGeoTarget(frame, 130.5F, 115.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 5) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 6U);
        EXPECT_FALSE(target.frameInfos[target.frameInfos.size() - 2U].valid);
        EXPECT_TRUE(target.frameInfos.back().valid);
        EXPECT_TRUE(target.living);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 130.5F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 115.0F);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.x, 4.0F);
        EXPECT_FLOAT_EQ(target.predictedSpdFrame.y, 3.0F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.x, 134.5F);
        EXPECT_FLOAT_EQ(target.predictedPosFrame.y, 118.0F);
    }
}

TEST(GeoTracker, RejectsTrackedMeasurementWhenFrameSpeedErrorIsTooLarge) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
        } else {
            addGeoTarget(frame, 133.0F, 116.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_FALSE(target.frameInfos.back().valid);
        EXPECT_TRUE(target.living);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 125.0F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 116.0F);
    }
}

TEST(GeoTracker, RejectsTrackedMeasurementWhenAePositionErrorExceedsThreshold) {
    auto settings = makeSettings();
    settings.thresholdAe = 0.001F;
    Dss::Tracking::GeoTracker tracker(settings);
    Dss::Core::Vec2f predictedAe{};

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);
        if (index == 4) {
            frame.targetBlobs.back().posAe = Dss::Core::Vec2f{0.8F, 0.9F};
        }

        const auto targets = tracker.track(frame);
        if (index == 3) {
            ASSERT_EQ(targets.size(), 1U);
            predictedAe = targets.front().predictedPosAe;
            continue;
        }

        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_FALSE(target.frameInfos.back().valid);
        EXPECT_TRUE(target.living);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.posAe.x, predictedAe.x);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.posAe.y, predictedAe.y);
    }
}

TEST(GeoTracker, TracksNonFullLeoTargetInRaDecSpace) {
    auto settings = makeSettings();
    settings.geoFullLeo = false;
    settings.geoRaSpeedThresholdArcsec = 60.0;
    settings.geoDecSpeedThresholdArcsec = 60.0;
    Dss::Tracking::GeoTracker tracker(settings);

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTargetWithRaDec(frame, 105.0F + static_cast<float>(index) * 5.0F,
                                  100.0F + static_cast<float>(index) * 4.0F,
                                  1.0 + static_cast<double>(index) * 0.0001,
                                  0.5 + static_cast<double>(index) * 0.00007);
        } else {
            addGeoTargetWithRaDec(frame, 900.0F, 900.0F, 1.0004, 0.50028);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_TRUE(target.frameInfos.back().valid);
        EXPECT_TRUE(target.living);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 900.0F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 900.0F);
        EXPECT_NEAR(target.predictedPosFrame.x, 1.0005F, 1.0e-6F);
        EXPECT_NEAR(target.predictedPosFrame.y, 0.50035F, 1.0e-6F);
    }
}

TEST(GeoTracker, EndsNonFullLeoTargetWhenPredictedRaDecLeavesSkyBounds) {
    auto settings = makeSettings();
    settings.geoFullLeo = false;
    settings.geoRaSpeedThresholdArcsec = 60.0;
    settings.geoDecSpeedThresholdArcsec = 60.0;
    Dss::Tracking::GeoTracker tracker(settings);

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTargetWithRaDec(frame, 105.0F + static_cast<float>(index) * 5.0F,
                                  100.0F + static_cast<float>(index) * 4.0F,
                                  6.2819 + static_cast<double>(index) * 0.0004,
                                  0.5 + static_cast<double>(index) * 0.00007);
        } else {
            addGeoTargetWithRaDec(frame, 900.0F, 900.0F, 6.2835, 0.50028);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_TRUE(target.frameInfos.back().valid);
        EXPECT_GT(target.predictedPosFrame.x, static_cast<float>(2.0 * Dss::Core::Pi));
        EXPECT_FALSE(target.living);
    }
}

TEST(GeoTracker, UsesExternalValidationBlobForInvalidGeoFrame) {
    Dss::Tracking::GeoTracker tracker(makeSettings());
    std::string targetId;

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
        } else {
            auto validationBlob = makeBlob(131.0F, 122.0F, 0.31F, 0.22F);
            validationBlob.id = targetId;
            validationBlob.alpha = 1.31;
            validationBlob.sigma = 1.22;
            frame.validatedTargetBlobs.push_back(validationBlob);
        }

        const auto targets = tracker.track(frame);
        if (index == 3) {
            ASSERT_EQ(targets.size(), 1U);
            targetId = targets.front().targetId;
            ASSERT_FALSE(targetId.empty());
            continue;
        }

        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_FALSE(target.frameInfos.back().valid);
        EXPECT_TRUE(target.living);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 131.0F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 122.0F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.posAe.x, 0.31F);
        EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.posAe.y, 0.22F);
    }
}

TEST(GeoTracker, UsesExternalValidationBlobOnlyForMatchingGeoTargetId) {
    Dss::Tracking::GeoTracker tracker(makeSettings());
    std::vector<Dss::Core::TargetInfo> associatedTargets;

    for (uint64_t index = 0; index < 4; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);
        addGeoTarget(frame, 300.0F + static_cast<float>(index) * 3.0F,
                     360.0F + static_cast<float>(index) * 5.0F);
        associatedTargets = tracker.track(frame);
    }

    ASSERT_EQ(associatedTargets.size(), 2U);
    const auto firstId = associatedTargets[0].targetId;
    const auto secondId = associatedTargets[1].targetId;
    ASSERT_NE(firstId, secondId);
    const auto secondPredicted = associatedTargets[1].predictedPosFrame;

    auto validationFrame = makeFrame(5, 4);
    addCenteredStars(validationFrame, 4.0F, 0.0F);
    auto matchedValidation = makeBlob(131.0F, 122.0F, 0.31F, 0.22F);
    matchedValidation.id = firstId;
    validationFrame.validatedTargetBlobs.push_back(matchedValidation);

    const auto trackedTargets = tracker.track(validationFrame);
    ASSERT_EQ(trackedTargets.size(), 2U);

    const auto firstTarget = std::ranges::find_if(
        trackedTargets,
        [&firstId](const Dss::Core::TargetInfo& target) { return target.targetId == firstId; });
    const auto secondTarget = std::ranges::find_if(
        trackedTargets,
        [&secondId](const Dss::Core::TargetInfo& target) { return target.targetId == secondId; });
    ASSERT_NE(firstTarget, trackedTargets.end());
    ASSERT_NE(secondTarget, trackedTargets.end());
    ASSERT_FALSE(firstTarget->frameInfos.empty());
    ASSERT_FALSE(secondTarget->frameInfos.empty());

    const auto& firstLatest = firstTarget->frameInfos.back();
    const auto& secondLatest = secondTarget->frameInfos.back();
    EXPECT_FALSE(firstLatest.valid);
    EXPECT_FLOAT_EQ(firstLatest.measuredBlob.centroid.x, matchedValidation.centroid.x);
    EXPECT_FLOAT_EQ(firstLatest.measuredBlob.centroid.y, matchedValidation.centroid.y);
    EXPECT_FALSE(secondLatest.valid);
    EXPECT_FLOAT_EQ(secondLatest.measuredBlob.centroid.x, secondPredicted.x);
    EXPECT_FLOAT_EQ(secondLatest.measuredBlob.centroid.y, secondPredicted.y);
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
            addGeoTarget(frame, 145.0F - static_cast<float>(index) * 5.0F,
                         136.0F - static_cast<float>(index) * 4.0F);
        } else {
            addGeoTarget(frame, 125.0F, 118.0F);
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
                EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.x, 125.0F);
                EXPECT_FLOAT_EQ(target.frameInfos.back().measuredBlob.centroid.y, 118.0F);
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

TEST(GeoTracker, AddsNewTargetWhileExistingTargetContinuesTracking) {
    Dss::Tracking::GeoTracker tracker(makeSettings());

    for (uint64_t index = 0; index < 8; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                     100.0F + static_cast<float>(index) * 4.0F);
        if (index >= 4) {
            const auto newTrackIndex = static_cast<float>(index - 4U);
            addGeoTarget(frame, 300.0F + newTrackIndex * 3.0F, 360.0F + newTrackIndex * 5.0F);
        }

        const auto targets = tracker.track(frame);
        if (index < 7) {
            continue;
        }

        ASSERT_EQ(targets.size(), 2U);
        const auto discovered =
            std::ranges::find_if(targets, [](const Dss::Core::TargetInfo& target) {
                return !target.frameInfos.empty() && target.frameInfos.front().frameSeq == 5U;
            });
        ASSERT_NE(discovered, targets.end());
        EXPECT_TRUE(discovered->living);
        ASSERT_EQ(discovered->frameInfos.size(), 4U);
        EXPECT_EQ(discovered->frameInfos.back().frameSeq, 8U);
        EXPECT_FLOAT_EQ(discovered->predictedSpdFrame.x, 3.0F);
        EXPECT_FLOAT_EQ(discovered->predictedSpdFrame.y, 5.0F);
        EXPECT_FLOAT_EQ(discovered->predictedPosFrame.x, 312.0F);
        EXPECT_FLOAT_EQ(discovered->predictedPosFrame.y, 380.0F);
    }
}

TEST(GeoTracker, EndsTrackedTargetWhenConsecutiveMeasurementsRepeatEquatorialPoint) {
    Dss::Tracking::GeoTracker tracker(makeSettings());
    double previousAlpha = 0.0;
    double previousSigma = 0.0;

    for (uint64_t index = 0; index < 5; ++index) {
        auto frame = makeFrame(index + 1, static_cast<int>(index));
        addCenteredStars(frame, static_cast<float>(index), 0.0F);
        if (index < 4) {
            addGeoTarget(frame, 105.0F + static_cast<float>(index) * 5.0F,
                         100.0F + static_cast<float>(index) * 4.0F);
            previousAlpha = frame.targetBlobs.back().alpha;
            previousSigma = frame.targetBlobs.back().sigma;
        } else {
            addGeoTargetWithEquatorial(frame, 125.0F, 116.0F, previousAlpha, previousSigma);
        }

        const auto targets = tracker.track(frame);
        if (index < 4) {
            continue;
        }

        ASSERT_EQ(targets.size(), 1U);
        const auto& target = targets.front();
        ASSERT_EQ(target.frameInfos.size(), 5U);
        EXPECT_TRUE(target.frameInfos.back().valid);
        EXPECT_FALSE(target.living);
    }
}
