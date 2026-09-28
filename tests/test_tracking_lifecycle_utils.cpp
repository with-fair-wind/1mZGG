#include <stdexcept>

#include <gtest/gtest.h>

#include "dss/tracking/support/lifecycle_utils.h"

namespace {

auto makeFrameInfo(bool valid) -> Dss::Core::TargetFrameInfo {
    Dss::Core::TargetFrameInfo info{};
    info.valid = valid;
    return info;
}

auto makeTarget(std::initializer_list<bool> validFrames) -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    for (const auto valid : validFrames) {
        target.frameInfos.push_back(makeFrameInfo(valid));
    }
    return target;
}

}  // namespace

TEST(TrackingLifecycleUtils, CountsRecentInvalidFramesInsideRequestedWindow) {
    const auto target = makeTarget({true, false, false, true, false});

    EXPECT_EQ(Dss::Tracking::countRecentInvalidFrames(target, 0), 0);
    EXPECT_EQ(Dss::Tracking::countRecentInvalidFrames(target, 2), 1);
    EXPECT_EQ(Dss::Tracking::countRecentInvalidFrames(target, 3), 2);
    EXPECT_EQ(Dss::Tracking::countRecentInvalidFrames(target, 10), 3);
    EXPECT_EQ(Dss::Tracking::countRecentInvalidFrames(Dss::Core::TargetInfo{}, 5), 0);
}

TEST(TrackingLifecycleUtils, DetectsRecentInvalidRunsWithFullWindowOrAvailableFrames) {
    const auto twoInvalidFrames = makeTarget({false, false});
    EXPECT_FALSE(Dss::Tracking::latestFramesAreAllInvalid(
        twoInvalidFrames, 3, Dss::Tracking::RecentFrameWindowMode::RequireFullWindow));
    EXPECT_TRUE(Dss::Tracking::latestFramesAreAllInvalid(
        twoInvalidFrames, 3, Dss::Tracking::RecentFrameWindowMode::UseAvailableFrames));

    const auto interruptedRun = makeTarget({false, true, false});
    EXPECT_FALSE(Dss::Tracking::latestFramesAreAllInvalid(
        interruptedRun, 3, Dss::Tracking::RecentFrameWindowMode::RequireFullWindow));

    const auto completeRun = makeTarget({true, false, false, false});
    EXPECT_TRUE(Dss::Tracking::latestFramesAreAllInvalid(
        completeRun, 3, Dss::Tracking::RecentFrameWindowMode::RequireFullWindow));
    EXPECT_FALSE(Dss::Tracking::latestFramesAreAllInvalid(
        Dss::Core::TargetInfo{}, 3, Dss::Tracking::RecentFrameWindowMode::UseAvailableFrames));
    EXPECT_FALSE(Dss::Tracking::latestFramesAreAllInvalid(
        completeRun, 0, Dss::Tracking::RecentFrameWindowMode::UseAvailableFrames));
}

TEST(TrackingLifecycleUtils, AppliesValidityWindowRuleAfterLatestFrame) {
    auto target = makeTarget({true, true, false});
    target.validity = 0.5F;

    EXPECT_FALSE(Dss::Tracking::latestFrameIsValid(Dss::Core::TargetInfo{}));
    EXPECT_FALSE(Dss::Tracking::latestFrameIsValid(target));
    EXPECT_FALSE(Dss::Tracking::passesRecentValidityRule(target, 3, 0.75F));

    target.validity = 0.8F;
    EXPECT_TRUE(Dss::Tracking::passesRecentValidityRule(target, 3, 0.75F));

    target.validity = 0.5F;
    target.frameInfos.back().valid = true;
    EXPECT_TRUE(Dss::Tracking::passesRecentValidityRule(target, 3, 0.75F));

    target.frameInfos.pop_back();
    target.frameInfos.back().valid = false;
    EXPECT_TRUE(Dss::Tracking::passesRecentValidityRule(target, 3, 0.75F));
    EXPECT_TRUE(Dss::Tracking::passesRecentValidityRule(target, 0, 0.75F));
}

TEST(TrackingLifecycleUtils, AppliesConfiguredTrackMissPolicyToLivingDecision) {
    auto target = makeTarget({true, true, true, false});
    target.validity = 0.75F;

    Dss::Tracking::TrackLivingRule validityWindowRule{};
    validityWindowRule.frameWindow = 4;
    validityWindowRule.threshold = 0.5F;
    validityWindowRule.missPolicy = Dss::Tracking::TrackMissPolicy::UseValidityWindow;
    EXPECT_TRUE(Dss::Tracking::targetRemainsLiving(target, validityWindowRule));

    Dss::Tracking::TrackLivingRule latestFrameRule{};
    latestFrameRule.frameWindow = 4;
    latestFrameRule.threshold = 0.5F;
    latestFrameRule.missPolicy = Dss::Tracking::TrackMissPolicy::RequireLatestValid;
    EXPECT_FALSE(Dss::Tracking::targetRemainsLiving(target, latestFrameRule));

    target.frameInfos.back().valid = true;
    EXPECT_TRUE(Dss::Tracking::targetRemainsLiving(target, latestFrameRule));
    EXPECT_FALSE(Dss::Tracking::targetRemainsLiving(Dss::Core::TargetInfo{}, validityWindowRule));
}

TEST(TrackingLifecycleUtils, AppliesConsecutiveInvalidWindowTrackMissPolicy) {
    Dss::Tracking::TrackLivingRule rule{};
    rule.frameWindow = 5;
    rule.missPolicy = Dss::Tracking::TrackMissPolicy::DropAfterConsecutiveInvalidFrames;

    EXPECT_TRUE(
        Dss::Tracking::targetRemainsLiving(makeTarget({true, false, false, false, false}), rule));
    EXPECT_FALSE(Dss::Tracking::targetRemainsLiving(
        makeTarget({true, false, false, false, false, false}), rule));
    EXPECT_TRUE(Dss::Tracking::targetRemainsLiving(makeTarget({false, false, false, false}), rule));
}

TEST(TrackingLifecycleUtils, UpdatesValidityFromLatestFrame) {
    auto target = makeTarget({true, true, true, false});
    target.validity = 1.0F;

    Dss::Tracking::updateValidityWithLatestFrame(target);

    EXPECT_FLOAT_EQ(target.validity, 0.75F);

    auto emptyTarget = Dss::Core::TargetInfo{};
    emptyTarget.validity = 0.25F;
    Dss::Tracking::updateValidityWithLatestFrame(emptyTarget);
    EXPECT_FLOAT_EQ(emptyTarget.validity, 0.25F);
}

TEST(TrackingLifecycleUtils, ValidityWeightUsesTotalHistoryBeyondPredictionWindow) {
    Dss::Core::TargetInfo target{};
    target.frameInfos.resize(100);
    target.validity = 0.75F;
    target.frameInfos.back().valid = false;
    Dss::Tracking::updateValidityWithLatestFrame(target);
    EXPECT_NEAR(target.validity, 0.7425F, 1e-6F);
    target.frameInfos.push_back(makeFrameInfo(true));
    Dss::Tracking::updateValidityWithLatestFrame(target);
    EXPECT_NEAR(target.validity, 75.25F / 101.0F, 1e-6F);
}

TEST(TrackingLifecycleUtils, BoundedHistoryMatchesFullHistoryValidityAndLivingRules) {
    for (const auto capacity : {4U, 5U, 17U}) {
        Dss::Core::TargetInfo full{};
        Dss::Core::TargetInfo bounded{};
        full.validity = bounded.validity = 0.375F;
        for (std::uint64_t seq = 1; seq <= 3000; ++seq) {
            // Isolated misses, runs longer than each window, and recovery.
            auto frame = makeFrameInfo(seq % 97U > 21U && seq % 11U != 0U);
            frame.frameSeq = seq;
            full.frameInfos.push_back(frame);
            bounded.frameInfos.push_back(frame);
            Dss::Tracking::updateValidityWithLatestFrame(full);
            Dss::Tracking::updateValidityWithLatestFrame(bounded);
            Dss::Tracking::retainRecentTargetFrames(bounded, capacity);
            ASSERT_LE(bounded.frameInfos.size(), capacity);
            ASSERT_EQ(bounded.totalFrameCount(), seq);
            ASSERT_FLOAT_EQ(bounded.validity, full.validity);
            ASSERT_EQ(Dss::Tracking::countRecentInvalidFrames(bounded, capacity),
                      Dss::Tracking::countRecentInvalidFrames(full, capacity));
            for (const auto mode : {Dss::Tracking::RecentFrameWindowMode::RequireFullWindow,
                                    Dss::Tracking::RecentFrameWindowMode::UseAvailableFrames}) {
                ASSERT_EQ(Dss::Tracking::latestFramesAreAllInvalid(bounded, capacity, mode),
                          Dss::Tracking::latestFramesAreAllInvalid(full, capacity, mode));
            }
            // Total-age gate must still work when greater than retained capacity.
            ASSERT_EQ(Dss::Tracking::passesRecentValidityRule(bounded, 100, 0.9F),
                      Dss::Tracking::passesRecentValidityRule(full, 100, 0.9F));
        }
        const auto capacityBefore = bounded.frameInfos.capacity();
        Dss::Tracking::retainRecentTargetFrames(bounded, capacity);
        EXPECT_EQ(bounded.totalFrameCount(), 3000U);
        EXPECT_EQ(bounded.frameInfos.capacity(), capacityBefore);
        bounded = {};
        EXPECT_EQ(bounded.totalFrameCount(), 0U);
    }
}

TEST(TrackingLifecycleUtils, RejectsZeroHistoryCapacityWithoutChangingTarget) {
    auto target = makeTarget({true, false, true});
    EXPECT_THROW(Dss::Tracking::retainRecentTargetFrames(target, 0), std::invalid_argument);
    EXPECT_EQ(target.totalFrameCount(), 3U);
    EXPECT_EQ(target.discardedFrameCount, 0U);
    Dss::Tracking::retainRecentTargetFrames(target, 2);
    EXPECT_EQ(target.totalFrameCount(), 3U);
    EXPECT_EQ(target.discardedFrameCount, 1U);
    EXPECT_FALSE(target.frameInfos.front().valid);
}
