#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dss/tracking/prediction_utils.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.opticParams.fovCenterX = 3072.0F;
    settings.opticParams.fovCenterY = 3072.0F;
    return settings;
}

auto makeBlob(std::string id, float x, float y, float azimuth, float elevation)
    -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.id = std::move(id);
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

auto makeFrame(uint64_t frameSeq, float frameFreq, std::vector<Dss::Core::MeasuredBlob> blobs)
    -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements frame{};
    frame.frameSeq = frameSeq;
    frame.fovCenterAe = Dss::Core::Vec2f{10.0F, 20.0F};
    frame.exposureTime = 0.125F;
    frame.frameFreq = frameFreq;
    frame.targetBlobs = std::move(blobs);
    return frame;
}

void expectVecNear(const Dss::Core::Vec2f& actual, const Dss::Core::Vec2f& expected) {
    EXPECT_NEAR(actual.x, expected.x, 1.0e-5F);
    EXPECT_NEAR(actual.y, expected.y, 1.0e-5F);
}

}  // namespace

TEST(TrackingPredictionUtils, BuildsValidAndInvalidFrameInfoFromMeasurements) {
    const auto settings = makeSettings();
    const auto frame = makeFrame(7U, 20.0F, {});
    const auto blob = makeBlob("blob-1", 123.0F, 456.0F, 1.5F, 2.5F);

    const auto info = Dss::Tracking::makeTargetFrameInfo(frame, blob, settings);

    EXPECT_EQ(info.frameSeq, 7U);
    expectVecNear(info.fovCenterAe, Dss::Core::Vec2f{10.0F, 20.0F});
    expectVecNear(info.opticCenter, Dss::Core::Vec2f{3072.0F, 3072.0F});
    EXPECT_FLOAT_EQ(info.exposureTime, 0.125F);
    EXPECT_FLOAT_EQ(info.frameFreq, 20.0F);
    EXPECT_EQ(info.measuredBlob.id, "blob-1");
    expectVecNear(info.measuredBlob.centroid, Dss::Core::Vec2f{123.0F, 456.0F});
    expectVecNear(info.posZxdw, Dss::Core::Vec2f{1.5F, 2.5F});
    expectVecNear(info.posTwdw, Dss::Core::Vec2f{1.5F, 2.5F});
    EXPECT_TRUE(info.valid);

    Dss::Core::TargetInfo target{};
    target.targetId = "tracked-1";
    target.predictedPosFrame = Dss::Core::Vec2f{200.0F, 300.0F};
    target.predictedPosAe = Dss::Core::Vec2f{3.0F, 4.0F};

    const auto invalidInfo = Dss::Tracking::makeInvalidTargetFrameInfo(frame, target, settings);

    EXPECT_EQ(invalidInfo.frameSeq, 7U);
    EXPECT_FALSE(invalidInfo.valid);
    EXPECT_EQ(invalidInfo.measuredBlob.id, "tracked-1");
    expectVecNear(invalidInfo.measuredBlob.centroid, Dss::Core::Vec2f{200.0F, 300.0F});
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.minX, 195.0F);
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.maxX, 205.0F);
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.minY, 295.0F);
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.maxY, 305.0F);
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.area, 0.0F);
    EXPECT_FLOAT_EQ(invalidInfo.measuredBlob.dn, 0.0F);
    expectVecNear(invalidInfo.measuredBlob.posAe, Dss::Core::Vec2f{3.0F, 4.0F});
}

TEST(TrackingPredictionUtils, FindsValidatedBlobForTargetById) {
    const std::vector<Dss::Core::MeasuredBlob> validatedBlobs{
        makeBlob("other", 10.0F, 20.0F, 1.0F, 2.0F),
        makeBlob("target-1", 30.0F, 40.0F, 3.0F, 4.0F),
    };

    const auto* matched = Dss::Tracking::findValidatedBlobForTarget(validatedBlobs, "target-1");

    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->id, "target-1");
    expectVecNear(matched->centroid, Dss::Core::Vec2f{30.0F, 40.0F});
    EXPECT_EQ(Dss::Tracking::findValidatedBlobForTarget(validatedBlobs, "missing"), nullptr);
}

TEST(TrackingPredictionUtils, BuildsInvalidFallbackBlobFromValidatedOrPrediction) {
    Dss::Core::TargetInfo target{};
    target.targetId = "target-1";
    target.predictedPosFrame = Dss::Core::Vec2f{22.0F, 16.0F};
    target.predictedPosAe = Dss::Core::Vec2f{1.6F, 2.9F};
    auto frame = makeFrame(4U, 1.0F, {});
    frame.validatedTargetBlobs.push_back(makeBlob("target-1", 30.0F, 40.0F, 3.0F, 4.0F));

    const auto validatedFallback = Dss::Tracking::makeInvalidFallbackBlob(frame, target);

    EXPECT_EQ(validatedFallback.id, "target-1");
    expectVecNear(validatedFallback.centroid, Dss::Core::Vec2f{30.0F, 40.0F});
    expectVecNear(validatedFallback.posAe, Dss::Core::Vec2f{3.0F, 4.0F});

    frame.validatedTargetBlobs.clear();
    Dss::Tracking::InvalidFallbackBlobOptions options{};
    options.halfExtent = 3.0F;
    options.copyPredictedFrameToRaDec = true;

    const auto predictedFallback = Dss::Tracking::makeInvalidFallbackBlob(frame, target, options);

    EXPECT_EQ(predictedFallback.id, "target-1");
    expectVecNear(predictedFallback.centroid, Dss::Core::Vec2f{22.0F, 16.0F});
    EXPECT_FLOAT_EQ(predictedFallback.minX, 19.0F);
    EXPECT_FLOAT_EQ(predictedFallback.maxX, 25.0F);
    EXPECT_FLOAT_EQ(predictedFallback.minY, 13.0F);
    EXPECT_FLOAT_EQ(predictedFallback.maxY, 19.0F);
    EXPECT_FLOAT_EQ(predictedFallback.area, 0.0F);
    EXPECT_FLOAT_EQ(predictedFallback.dn, 0.0F);
    EXPECT_DOUBLE_EQ(predictedFallback.ra, 22.0);
    EXPECT_DOUBLE_EQ(predictedFallback.dec, 16.0);
}

TEST(TrackingPredictionUtils, UsesValidatedBlobForInvalidTargetFrameInfo) {
    const auto settings = makeSettings();
    Dss::Core::TargetInfo target{};
    target.targetId = "target-1";
    target.predictedPosFrame = Dss::Core::Vec2f{22.0F, 16.0F};
    target.predictedPosAe = Dss::Core::Vec2f{1.6F, 2.9F};
    auto frame = makeFrame(4U, 1.0F, {});
    frame.validatedTargetBlobs.push_back(makeBlob("target-1", 30.0F, 40.0F, 3.0F, 4.0F));

    const auto invalidInfo = Dss::Tracking::makeInvalidTargetFrameInfo(frame, target, settings);

    EXPECT_FALSE(invalidInfo.valid);
    EXPECT_EQ(invalidInfo.measuredBlob.id, "target-1");
    expectVecNear(invalidInfo.measuredBlob.centroid, Dss::Core::Vec2f{30.0F, 40.0F});
    expectVecNear(invalidInfo.measuredBlob.posAe, Dss::Core::Vec2f{3.0F, 4.0F});
}

TEST(TrackingPredictionUtils, UpdatesPredictionFromRecentFourWithMedianMotion) {
    const auto settings = makeSettings();
    Dss::Core::TargetInfo target{};
    target.targetId = "target-1";
    target.validity = 1.0F;

    const auto firstFrame = makeFrame(1U, 1.0F, {});
    const auto secondFrame = makeFrame(2U, 1.0F, {});
    const auto thirdFrame = makeFrame(3U, 1.0F, {});
    const auto fourthFrame = makeFrame(4U, 1.0F, {});
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        firstFrame, makeBlob("a", 10.0F, 10.0F, 1.00F, 2.00F), settings));
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        secondFrame, makeBlob("b", 14.0F, 14.0F, 1.10F, 2.20F), settings));
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        thirdFrame, makeBlob("c", 22.0F, 16.0F, 1.50F, 2.30F), settings));
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        fourthFrame, makeBlob("d", 28.0F, 19.0F, 1.80F, 2.60F), settings));

    Dss::Tracking::updatePredictionFromRecentFour(target);

    expectVecNear(target.predictedSpdFrame, Dss::Core::Vec2f{6.0F, 3.0F});
    expectVecNear(target.predictedPosFrame, Dss::Core::Vec2f{34.0F, 22.0F});
    expectVecNear(target.predictedSpdAe, Dss::Core::Vec2f{0.3F, 0.2F});
    expectVecNear(target.predictedPosAe, Dss::Core::Vec2f{2.1F, 2.8F});
    EXPECT_FLOAT_EQ(target.validity, 1.0F);
}

TEST(TrackingPredictionUtils, FindsNearestBlobInFrameOrAeSpaceWithOptionalFovGate) {
    const auto settings = makeSettings();
    const auto frame = makeFrame(10U, 10.0F,
                                 {makeBlob("outside-near", 3200.0F, 3072.0F, 5.0F, 6.0F),
                                  makeBlob("inside-near", 3070.0F, 3071.0F, 1.0F, 2.0F),
                                  makeBlob("inside-ae", 3060.0F, 3060.0F, 4.9F, 6.1F)});

    Dss::Core::TargetInfo target{};
    target.predictedPosFrame = Dss::Core::Vec2f{3200.0F, 3072.0F};
    target.predictedPosAe = Dss::Core::Vec2f{5.0F, 6.0F};

    Dss::Tracking::BlobMatchOptions frameOptions{};
    frameOptions.space = Dss::Tracking::BlobMatchSpace::Frame;
    frameOptions.threshold = 3.0F;
    const auto* frameMatch = Dss::Tracking::findNearestBlob(frame, target, settings, frameOptions);
    ASSERT_NE(frameMatch, nullptr);
    EXPECT_EQ(frameMatch->id, "outside-near");

    frameOptions.requireFovCenter = true;
    frameOptions.fovCenterHalfExtent = 128.0F;
    const auto* gatedFrameMatch =
        Dss::Tracking::findNearestBlob(frame, target, settings, frameOptions);
    EXPECT_EQ(gatedFrameMatch, nullptr);

    Dss::Tracking::BlobMatchOptions aeOptions{};
    aeOptions.space = Dss::Tracking::BlobMatchSpace::Ae;
    aeOptions.threshold = 0.2F;
    const auto* aeMatch = Dss::Tracking::findNearestBlob(frame, target, settings, aeOptions);
    ASSERT_NE(aeMatch, nullptr);
    EXPECT_EQ(aeMatch->id, "outside-near");
}

TEST(TrackingPredictionUtils, AppendsMatchedFrameAndUpdatesPrediction) {
    const auto settings = makeSettings();
    Dss::Core::TargetInfo target{};
    target.targetId = "target-1";
    target.validity = 1.0F;
    target.predictedPosFrame = Dss::Core::Vec2f{22.0F, 16.0F};
    target.predictedPosAe = Dss::Core::Vec2f{1.6F, 2.9F};
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        makeFrame(1U, 1.0F, {}), makeBlob("a", 10.0F, 10.0F, 1.0F, 2.0F), settings));
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        makeFrame(2U, 1.0F, {}), makeBlob("b", 14.0F, 12.0F, 1.2F, 2.3F), settings));
    target.frameInfos.push_back(Dss::Tracking::makeTargetFrameInfo(
        makeFrame(3U, 1.0F, {}), makeBlob("c", 18.0F, 14.0F, 1.4F, 2.6F), settings));

    const auto frame = makeFrame(4U, 1.0F, {makeBlob("match", 22.0F, 16.0F, 1.6F, 2.9F)});
    Dss::Tracking::BlobMatchOptions options{};
    options.space = Dss::Tracking::BlobMatchSpace::Frame;
    options.threshold = 1.0F;

    const auto updated =
        Dss::Tracking::appendMatchedFrameAndUpdatePrediction(frame, target, settings, options);

    ASSERT_EQ(updated.frameInfos.size(), 4U);
    EXPECT_TRUE(updated.frameInfos.back().valid);
    EXPECT_EQ(updated.frameInfos.back().measuredBlob.id, "match");
    expectVecNear(updated.predictedSpdFrame, Dss::Core::Vec2f{4.0F, 2.0F});
    expectVecNear(updated.predictedPosFrame, Dss::Core::Vec2f{26.0F, 18.0F});
    expectVecNear(updated.predictedSpdAe, Dss::Core::Vec2f{0.2F, 0.3F});
    expectVecNear(updated.predictedPosAe, Dss::Core::Vec2f{1.8F, 3.2F});
}

TEST(TrackingPredictionUtils, AppendsInvalidFrameWhenNoBlobMatches) {
    const auto settings = makeSettings();
    Dss::Core::TargetInfo target{};
    target.targetId = "target-1";
    target.predictedPosFrame = Dss::Core::Vec2f{22.0F, 16.0F};
    target.predictedPosAe = Dss::Core::Vec2f{1.6F, 2.9F};

    const auto frame = makeFrame(4U, 1.0F, {});
    Dss::Tracking::BlobMatchOptions options{};
    options.space = Dss::Tracking::BlobMatchSpace::Frame;
    options.threshold = 1.0F;

    const auto updated =
        Dss::Tracking::appendMatchedFrameAndUpdatePrediction(frame, target, settings, options);

    ASSERT_EQ(updated.frameInfos.size(), 1U);
    const auto& invalidFrame = updated.frameInfos.back();
    EXPECT_FALSE(invalidFrame.valid);
    EXPECT_EQ(invalidFrame.measuredBlob.id, "target-1");
    expectVecNear(invalidFrame.measuredBlob.centroid, Dss::Core::Vec2f{22.0F, 16.0F});
    expectVecNear(invalidFrame.measuredBlob.posAe, Dss::Core::Vec2f{1.6F, 2.9F});
}
