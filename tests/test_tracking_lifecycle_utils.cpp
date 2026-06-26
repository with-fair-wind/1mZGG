#include <gtest/gtest.h>

#include "dss/tracking/lifecycle_utils.h"

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
