#include "dss/tracking/support/lifecycle_utils.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace {

[[nodiscard]] auto clampedRecentFrameCount(const Dss::Core::TargetInfo& target, int frameWindow)
    -> std::size_t {
    if (frameWindow <= 0 || target.frameInfos.empty()) {
        return 0U;
    }

    return std::min(target.frameInfos.size(), static_cast<std::size_t>(frameWindow));
}

}  // namespace

namespace Dss::Tracking {

void retainRecentTargetFrames(Core::TargetInfo& target, std::size_t maxFrames) {
    if (maxFrames == 0) {
        throw std::invalid_argument("target history capacity must be positive");
    }
    if (target.frameInfos.size() <= maxFrames) {
        return;
    }
    const auto removed = target.frameInfos.size() - maxFrames;
    target.frameInfos.erase(target.frameInfos.begin(),
                            target.frameInfos.begin() + static_cast<std::ptrdiff_t>(removed));
    target.discardedFrameCount += static_cast<std::uint64_t>(removed);
}

/// 统计最近窗口内无效帧数量
auto countRecentInvalidFrames(const Core::TargetInfo& target, int frameWindow) -> int {
    const auto checkCount = clampedRecentFrameCount(target, frameWindow);
    if (checkCount == 0U) {
        return 0;
    }

    return static_cast<int>(std::ranges::count_if(
        target.frameInfos.end() - static_cast<std::ptrdiff_t>(checkCount), target.frameInfos.end(),
        [](const Core::TargetFrameInfo& info) { return !info.valid; }));
}

bool latestFramesAreAllInvalid(const Core::TargetInfo& target, int frameWindow,
                               RecentFrameWindowMode mode) {
    const auto checkCount = clampedRecentFrameCount(target, frameWindow);
    if (checkCount == 0U) {
        return false;
    }
    if (mode == RecentFrameWindowMode::RequireFullWindow &&
        target.frameInfos.size() < static_cast<std::size_t>(frameWindow)) {
        return false;
    }

    return std::all_of(target.frameInfos.end() - static_cast<std::ptrdiff_t>(checkCount),
                       target.frameInfos.end(),
                       [](const Core::TargetFrameInfo& info) { return !info.valid; });
}

bool latestFrameIsValid(const Core::TargetInfo& target) {
    return !target.frameInfos.empty() && target.frameInfos.back().valid;
}

bool passesRecentValidityRule(const Core::TargetInfo& target, int frameWindow, float threshold) {
    if (target.frameInfos.empty()) {
        return false;
    }
    if (frameWindow <= 0 || target.totalFrameCount() < static_cast<std::uint64_t>(frameWindow)) {
        return true;
    }

    return target.validity >= threshold || latestFrameIsValid(target);
}

/// 按 missPolicy 策略综合判定目标是否仍存活
bool targetRemainsLiving(const Core::TargetInfo& target, const TrackLivingRule& rule) {
    if (target.frameInfos.empty()) {
        return false;
    }

    switch (rule.missPolicy) {
        case TrackMissPolicy::UseValidityWindow:
            return passesRecentValidityRule(target, rule.frameWindow, rule.threshold);
        case TrackMissPolicy::RequireLatestValid:
            return latestFrameIsValid(target);
        case TrackMissPolicy::DropAfterConsecutiveInvalidFrames:
            return !latestFramesAreAllInvalid(target, rule.frameWindow,
                                              RecentFrameWindowMode::RequireFullWindow);
    }
    return false;
}

/// 按累计样本数加权，保持旧算法的初始有效率与浮点运算顺序。
void updateValidityWithLatestFrame(Core::TargetInfo& target) {
    if (target.frameInfos.empty()) {
        return;
    }

    const auto sampleCount = static_cast<float>(target.totalFrameCount());
    const auto latestValid = latestFrameIsValid(target) ? 1.0F : 0.0F;
    target.validity = ((sampleCount - 1.0F) * target.validity + latestValid) / sampleCount;
}

}  // namespace Dss::Tracking
