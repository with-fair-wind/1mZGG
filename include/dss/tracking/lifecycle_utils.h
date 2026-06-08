#pragma once

#include "dss/core/types.h"

namespace Dss::Tracking {

enum class RecentFrameWindowMode {
    RequireFullWindow,
    UseAvailableFrames,
};

enum class TrackMissPolicy {
    UseValidityWindow,
    RequireLatestValid,
    DropAfterConsecutiveInvalidFrames,
};

struct TrackLivingRule {
    int frameWindow = 0;
    float threshold = 0.0F;
    TrackMissPolicy missPolicy = TrackMissPolicy::UseValidityWindow;
};

[[nodiscard]] auto countRecentInvalidFrames(const Core::TargetInfo& target, int frameWindow) -> int;

[[nodiscard]] bool latestFramesAreAllInvalid(const Core::TargetInfo& target, int frameWindow,
                                             RecentFrameWindowMode mode);

[[nodiscard]] bool latestFrameIsValid(const Core::TargetInfo& target);

[[nodiscard]] bool passesRecentValidityRule(const Core::TargetInfo& target, int frameWindow,
                                            float threshold);

[[nodiscard]] bool targetRemainsLiving(const Core::TargetInfo& target, const TrackLivingRule& rule);

void updateValidityWithLatestFrame(Core::TargetInfo& target);

}  // namespace Dss::Tracking
