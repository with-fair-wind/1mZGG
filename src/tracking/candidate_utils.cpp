#include "dss/tracking/candidate_utils.h"

#include <utility>
#include <vector>

namespace {

[[nodiscard]] bool hasEnoughInitialFrames(const Dss::Core::TargetInfo& target,
                                          std::size_t frameCount) {
    return frameCount > 0U && target.frameInfos.size() >= frameCount;
}

[[nodiscard]] bool hasSameCentroid(const Dss::Core::TargetFrameInfo& first,
                                   const Dss::Core::TargetFrameInfo& second) {
    return first.measuredBlob.centroid.x == second.measuredBlob.centroid.x &&
           first.measuredBlob.centroid.y == second.measuredBlob.centroid.y;
}

}  // namespace

namespace Dss::Tracking {

/// 判断两个候选在指定初始帧窗口内是否存在同帧同质心测量
bool sharesInitialCentroidAtSameFrameIndex(const Core::TargetInfo& first,
                                           const Core::TargetInfo& second,
                                           const InitialMeasurementDedupRule& rule) {
    if (!hasEnoughInitialFrames(first, rule.frameCount) ||
        !hasEnoughInitialFrames(second, rule.frameCount)) {
        return false;
    }

    for (std::size_t frameIndex = 0U; frameIndex < rule.frameCount; ++frameIndex) {
        if (hasSameCentroid(first.frameInfos[frameIndex], second.frameInfos[frameIndex])) {
            return true;
        }
    }
    return false;
}

/// 两两比对候选并移除重复项，保留先出现的候选
auto deduplicateInitialCandidatesByCentroid(std::vector<Core::TargetInfo> candidates,
                                            const InitialMeasurementDedupRule& rule)
    -> std::vector<Core::TargetInfo> {
    if (candidates.size() < 2U || rule.frameCount == 0U) {
        return candidates;
    }

    std::vector<bool> shouldRemove(candidates.size(), false);
    for (std::size_t firstIndex = 0U; firstIndex < candidates.size(); ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1U; secondIndex < candidates.size();
             ++secondIndex) {
            if (sharesInitialCentroidAtSameFrameIndex(candidates[firstIndex],
                                                      candidates[secondIndex], rule)) {
                shouldRemove[secondIndex] = true;
            }
        }
    }

    std::vector<Core::TargetInfo> deduplicated;
    deduplicated.reserve(candidates.size());
    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        if (!shouldRemove[index]) {
            deduplicated.push_back(std::move(candidates[index]));
        }
    }
    return deduplicated;
}

}  // namespace Dss::Tracking
