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
