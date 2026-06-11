#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/tracking/candidate_utils.h"

namespace {

struct MeasurementPoint {
    Dss::Core::Vec2f centroid{};
    Dss::Core::Vec2d raDec{};
};

auto makeTarget(std::string targetId, std::initializer_list<Dss::Core::Vec2f> centroids)
    -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.targetId = std::move(targetId);
    auto frameSeq = 1U;
    for (const auto& centroid : centroids) {
        Dss::Core::TargetFrameInfo info{};
        info.frameSeq = frameSeq;
        info.valid = true;
        info.measuredBlob.centroid = centroid;
        target.frameInfos.push_back(info);
        ++frameSeq;
    }
    return target;
}

auto makeTargetWithRaDec(std::string targetId, std::initializer_list<MeasurementPoint> points,
                         std::uint64_t firstFrameSeq = 1U) -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.targetId = std::move(targetId);
    auto frameSeq = firstFrameSeq;
    for (const auto& point : points) {
        Dss::Core::TargetFrameInfo info{};
        info.frameSeq = frameSeq;
        info.valid = true;
        info.measuredBlob.centroid = point.centroid;
        info.measuredBlob.ra = point.raDec.x;
        info.measuredBlob.dec = point.raDec.y;
        target.frameInfos.push_back(info);
        ++frameSeq;
    }
    return target;
}

auto makeTargetWithFrameSeq(std::string targetId, std::initializer_list<Dss::Core::Vec2f> centroids,
                            std::uint64_t firstFrameSeq) -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.targetId = std::move(targetId);
    auto frameSeq = firstFrameSeq;
    for (const auto& centroid : centroids) {
        Dss::Core::TargetFrameInfo info{};
        info.frameSeq = frameSeq;
        info.valid = true;
        info.measuredBlob.centroid = centroid;
        target.frameInfos.push_back(info);
        ++frameSeq;
    }
    return target;
}

}  // namespace

TEST(TrackingCandidateUtils, DetectsSharedInitialCentroidByMatchingFrameIndex) {
    Dss::Tracking::InitialMeasurementDedupRule rule{};
    rule.frameCount = 3U;

    const auto first = makeTarget("first", {{1.0F, 1.0F}, {2.0F, 2.0F}, {3.0F, 3.0F}});
    const auto sharesSecondFrame =
        makeTarget("shares-second", {{9.0F, 9.0F}, {2.0F, 2.0F}, {8.0F, 8.0F}});
    EXPECT_TRUE(
        Dss::Tracking::sharesInitialCentroidAtSameFrameIndex(first, sharesSecondFrame, rule));

    const auto sameCentroidAtDifferentIndex =
        makeTarget("different-index", {{2.0F, 2.0F}, {9.0F, 9.0F}, {8.0F, 8.0F}});
    EXPECT_FALSE(Dss::Tracking::sharesInitialCentroidAtSameFrameIndex(
        first, sameCentroidAtDifferentIndex, rule));

    const auto incomplete = makeTarget("incomplete", {{1.0F, 1.0F}, {2.0F, 2.0F}});
    EXPECT_FALSE(Dss::Tracking::sharesInitialCentroidAtSameFrameIndex(first, incomplete, rule));

    rule.frameCount = 0U;
    EXPECT_FALSE(
        Dss::Tracking::sharesInitialCentroidAtSameFrameIndex(first, sharesSecondFrame, rule));
}

TEST(TrackingCandidateUtils, DeduplicatesAgainstOriginalCandidateSetAndKeepsFirstCandidate) {
    Dss::Tracking::InitialMeasurementDedupRule rule{};
    rule.frameCount = 3U;

    auto first = makeTarget("first", {{1.0F, 1.0F}, {2.0F, 2.0F}, {3.0F, 3.0F}});
    auto second = makeTarget("second", {{1.0F, 1.0F}, {20.0F, 20.0F}, {30.0F, 30.0F}});
    auto third = makeTarget("third", {{10.0F, 10.0F}, {20.0F, 20.0F}, {31.0F, 31.0F}});
    auto independent =
        makeTarget("independent", {{100.0F, 100.0F}, {200.0F, 200.0F}, {300.0F, 300.0F}});

    auto candidates = std::vector<Dss::Core::TargetInfo>{std::move(first), std::move(second),
                                                         std::move(third), std::move(independent)};

    const auto deduplicated =
        Dss::Tracking::deduplicateInitialCandidatesByCentroid(std::move(candidates), rule);

    ASSERT_EQ(deduplicated.size(), 2U);
    EXPECT_EQ(deduplicated[0].targetId, "first");
    EXPECT_EQ(deduplicated[1].targetId, "independent");
}

TEST(TrackingCandidateUtils, DetectsSharedInitialRaDecByMatchingFrameIndex) {
    Dss::Tracking::InitialMeasurementDedupRule rule{};
    rule.frameCount = 3U;

    const auto first = makeTargetWithRaDec("first", {{{1.0F, 1.0F}, {1.0, 0.5}},
                                                     {{2.0F, 2.0F}, {1.0001, 0.5001}},
                                                     {{3.0F, 3.0F}, {1.0002, 0.5002}}});
    const auto sharesSecondRaDec =
        makeTargetWithRaDec("shares-second-radec", {{{10.0F, 10.0F}, {9.0, 9.0}},
                                                    {{20.0F, 20.0F}, {1.0001, 0.5001}},
                                                    {{30.0F, 30.0F}, {8.0, 8.0}}});

    EXPECT_FALSE(
        Dss::Tracking::sharesInitialCentroidAtSameFrameIndex(first, sharesSecondRaDec, rule));
    EXPECT_TRUE(Dss::Tracking::sharesInitialMeasurementAtSameFrameIndex(
        first, sharesSecondRaDec, rule, Dss::Tracking::CandidateMeasurementSpace::RaDec));
}

TEST(TrackingCandidateUtils, DeduplicatesInitialCandidatesByRaDecAndKeepsFirstCandidate) {
    Dss::Tracking::InitialMeasurementDedupRule rule{};
    rule.frameCount = 3U;

    auto first = makeTargetWithRaDec("first", {{{1.0F, 1.0F}, {1.0, 0.5}},
                                               {{2.0F, 2.0F}, {1.0001, 0.5001}},
                                               {{3.0F, 3.0F}, {1.0002, 0.5002}}});
    auto sameRaDecDifferentPixels =
        makeTargetWithRaDec("same-radec-different-pixels", {{{100.0F, 100.0F}, {1.0, 0.5}},
                                                            {{200.0F, 200.0F}, {9.0, 9.0}},
                                                            {{300.0F, 300.0F}, {8.0, 8.0}}});
    auto independent = makeTargetWithRaDec("independent", {{{400.0F, 400.0F}, {4.0, 0.4}},
                                                           {{500.0F, 500.0F}, {5.0, 0.5}},
                                                           {{600.0F, 600.0F}, {6.0, 0.6}}});

    auto candidates = std::vector<Dss::Core::TargetInfo>{
        std::move(first), std::move(sameRaDecDifferentPixels), std::move(independent)};

    const auto deduplicated = Dss::Tracking::deduplicateInitialCandidates(
        std::move(candidates), rule, Dss::Tracking::CandidateMeasurementSpace::RaDec);

    ASSERT_EQ(deduplicated.size(), 2U);
    EXPECT_EQ(deduplicated[0].targetId, "first");
    EXPECT_EQ(deduplicated[1].targetId, "independent");
}

TEST(TrackingCandidateUtils, DetectsRecentCentroidOverlapByMatchingFrameIndex) {
    Dss::Tracking::RecentMeasurementOverlapRule rule{};
    rule.frameCount = 3U;
    rule.space = Dss::Tracking::CandidateMeasurementSpace::Centroid;

    const auto candidate =
        makeTargetWithFrameSeq("candidate", {{20.0F, 20.0F}, {3.0F, 3.0F}, {40.0F, 40.0F}}, 12U);
    const auto existing = makeTargetWithFrameSeq(
        "existing", {{0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}, {3.0F, 3.0F}, {4.0F, 4.0F}}, 10U);

    EXPECT_TRUE(Dss::Tracking::sharesRecentMeasurementAtSameFrameIndex(candidate, existing, rule));

    const auto sameCentroidAtDifferentFrame = makeTargetWithFrameSeq(
        "different-frame", {{20.0F, 20.0F}, {3.0F, 3.0F}, {40.0F, 40.0F}}, 11U);
    EXPECT_FALSE(Dss::Tracking::sharesRecentMeasurementAtSameFrameIndex(
        sameCentroidAtDifferentFrame, existing, rule));
}

TEST(TrackingCandidateUtils, DetectsRecentRaDecOverlapWithoutPixelOverlap) {
    Dss::Tracking::RecentMeasurementOverlapRule rule{};
    rule.frameCount = 3U;
    rule.space = Dss::Tracking::CandidateMeasurementSpace::RaDec;

    const auto candidate = makeTargetWithRaDec(
        "candidate",
        {{{20.0F, 20.0F}, {9.0, 9.0}}, {{30.0F, 30.0F}, {1.2, 0.7}}, {{40.0F, 40.0F}, {8.0, 8.0}}},
        12U);
    const auto existing = makeTargetWithRaDec("existing",
                                              {{{0.0F, 0.0F}, {0.1, 0.1}},
                                               {{1.0F, 1.0F}, {0.2, 0.2}},
                                               {{2.0F, 2.0F}, {0.3, 0.3}},
                                               {{3.0F, 3.0F}, {1.2, 0.7}},
                                               {{4.0F, 4.0F}, {0.4, 0.4}}},
                                              10U);

    EXPECT_TRUE(Dss::Tracking::sharesRecentMeasurementAtSameFrameIndex(candidate, existing, rule));

    rule.space = Dss::Tracking::CandidateMeasurementSpace::Centroid;
    EXPECT_FALSE(Dss::Tracking::sharesRecentMeasurementAtSameFrameIndex(candidate, existing, rule));
}

TEST(TrackingCandidateUtils, OverlapsOnlyLivingTargetsInRecentWindow) {
    Dss::Tracking::RecentMeasurementOverlapRule rule{};
    rule.frameCount = 3U;
    rule.space = Dss::Tracking::CandidateMeasurementSpace::Centroid;

    const auto candidate =
        makeTargetWithFrameSeq("candidate", {{20.0F, 20.0F}, {3.0F, 3.0F}, {40.0F, 40.0F}}, 12U);
    auto nonLiving = makeTargetWithFrameSeq(
        "non-living", {{0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}, {3.0F, 3.0F}, {4.0F, 4.0F}}, 10U);
    nonLiving.living = false;
    auto living = makeTargetWithFrameSeq(
        "living", {{50.0F, 50.0F}, {51.0F, 51.0F}, {52.0F, 52.0F}, {53.0F, 53.0F}, {54.0F, 54.0F}},
        10U);
    living.living = true;

    const std::vector<Dss::Core::TargetInfo> withoutLivingOverlap{nonLiving, living};
    EXPECT_FALSE(Dss::Tracking::overlapsAnyLivingTarget(candidate, withoutLivingOverlap, rule));

    living.frameInfos[3].measuredBlob.centroid = Dss::Core::Vec2f{3.0F, 3.0F};
    const std::vector<Dss::Core::TargetInfo> withLivingOverlap{nonLiving, living};
    EXPECT_TRUE(Dss::Tracking::overlapsAnyLivingTarget(candidate, withLivingOverlap, rule));
}
