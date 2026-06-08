#include <initializer_list>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/tracking/candidate_utils.h"

namespace {

auto makeTarget(std::string targetId, std::initializer_list<Dss::Core::Vec2f> centroids)
    -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.targetId = std::move(targetId);
    for (const auto& centroid : centroids) {
        Dss::Core::TargetFrameInfo info{};
        info.valid = true;
        info.measuredBlob.centroid = centroid;
        target.frameInfos.push_back(info);
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
