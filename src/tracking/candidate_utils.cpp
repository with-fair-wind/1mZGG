#include "dss/tracking/candidate_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] bool hasEnoughInitialFrames(const Dss::Core::TargetInfo& target,
                                          std::size_t frameCount) {
    return frameCount > 0U && target.frameInfos.size() >= frameCount;
}

}  // namespace

namespace Dss::Tracking {

bool hasRaDecMeasurement(const Core::MeasuredBlob& blob) {
    return blob.ra != 0.0 || blob.dec != 0.0;
}

auto measurementPosition(const Core::MeasuredBlob& blob, CandidateMeasurementSpace space)
    -> std::optional<Core::Vec2f> {
    if (space == CandidateMeasurementSpace::RaDec) {
        if (!hasRaDecMeasurement(blob)) {
            return std::nullopt;
        }
        return Core::Vec2f{static_cast<float>(blob.ra), static_cast<float>(blob.dec)};
    }
    return blob.centroid;
}

auto measurementMotion(const Core::MeasuredBlob& current, const Core::MeasuredBlob& previous,
                       CandidateMeasurementSpace space) -> std::optional<Core::Vec2f> {
    const auto currentPosition = measurementPosition(current, space);
    const auto previousPosition = measurementPosition(previous, space);
    if (!currentPosition.has_value() || !previousPosition.has_value()) {
        return std::nullopt;
    }
    return Core::Vec2f{currentPosition->x - previousPosition->x,
                       currentPosition->y - previousPosition->y};
}

bool hasReusedMeasurement(const Core::MeasuredBlob& candidate, const Core::MeasuredBlob& used,
                          const MeasurementReuseRule& rule) {
    if (rule.space == CandidateMeasurementSpace::RaDec && hasRaDecMeasurement(candidate) &&
        hasRaDecMeasurement(used)) {
        if (rule.matchAnyRaDecAxis) {
            return candidate.ra == used.ra || candidate.dec == used.dec;
        }
        return candidate.ra == used.ra && candidate.dec == used.dec;
    }

    return candidate.centroid.x == used.centroid.x && candidate.centroid.y == used.centroid.y;
}

bool isMeasurementAlreadyUsed(const Core::MeasuredBlob& candidate,
                              std::span<const Core::MeasuredBlob> usedBlobs,
                              const MeasurementReuseRule& rule) {
    return std::ranges::any_of(usedBlobs, [&](const Core::MeasuredBlob& used) {
        return hasReusedMeasurement(candidate, used, rule);
    });
}

bool hasSameMeasurement(const Core::MeasuredBlob& first, const Core::MeasuredBlob& second,
                        CandidateMeasurementSpace space) {
    if (space == CandidateMeasurementSpace::RaDec) {
        return hasRaDecMeasurement(first) && hasRaDecMeasurement(second) && first.ra == second.ra &&
               first.dec == second.dec;
    }
    return first.centroid.x == second.centroid.x && first.centroid.y == second.centroid.y;
}

bool hasSameMeasurement(const Core::TargetFrameInfo& first, const Core::TargetFrameInfo& second,
                        CandidateMeasurementSpace space) {
    return hasSameMeasurement(first.measuredBlob, second.measuredBlob, space);
}

bool sharesInitialMeasurementAtSameFrameIndex(const Core::TargetInfo& first,
                                              const Core::TargetInfo& second,
                                              const InitialMeasurementDedupRule& rule,
                                              CandidateMeasurementSpace space) {
    if (!hasEnoughInitialFrames(first, rule.frameCount) ||
        !hasEnoughInitialFrames(second, rule.frameCount)) {
        return false;
    }

    for (std::size_t frameIndex = 0U; frameIndex < rule.frameCount; ++frameIndex) {
        if (hasSameMeasurement(first.frameInfos[frameIndex], second.frameInfos[frameIndex],
                               space)) {
            return true;
        }
    }
    return false;
}

bool sharesInitialCentroidAtSameFrameIndex(const Core::TargetInfo& first,
                                           const Core::TargetInfo& second,
                                           const InitialMeasurementDedupRule& rule) {
    return sharesInitialMeasurementAtSameFrameIndex(first, second, rule,
                                                    CandidateMeasurementSpace::Centroid);
}

bool sharesRecentMeasurementAtSameFrameIndex(const Core::TargetInfo& candidate,
                                             const Core::TargetInfo& existingTarget,
                                             const RecentMeasurementOverlapRule& rule) {
    if (rule.frameCount == 0U || candidate.frameInfos.size() < rule.frameCount ||
        existingTarget.frameInfos.size() < rule.frameCount) {
        return false;
    }

    const auto candidateStart = candidate.frameInfos.size() - rule.frameCount;
    const auto existingStart = existingTarget.frameInfos.size() - rule.frameCount;
    for (std::size_t offset = 0U; offset < rule.frameCount; ++offset) {
        const auto& candidateInfo = candidate.frameInfos[candidateStart + offset];
        const auto& existingInfo = existingTarget.frameInfos[existingStart + offset];
        if (candidateInfo.frameSeq == existingInfo.frameSeq &&
            hasSameMeasurement(candidateInfo, existingInfo, rule.space)) {
            return true;
        }
    }
    return false;
}

bool overlapsAnyLivingTarget(const Core::TargetInfo& candidate,
                             std::span<const Core::TargetInfo> targets,
                             const RecentMeasurementOverlapRule& rule) {
    return std::ranges::any_of(targets, [&](const Core::TargetInfo& target) {
        return target.living && sharesRecentMeasurementAtSameFrameIndex(candidate, target, rule);
    });
}

auto deduplicateInitialCandidates(std::vector<Core::TargetInfo> candidates,
                                  const InitialMeasurementDedupRule& rule,
                                  CandidateMeasurementSpace space)
    -> std::vector<Core::TargetInfo> {
    if (candidates.size() < 2U || rule.frameCount == 0U) {
        return candidates;
    }

    std::vector<bool> shouldRemove(candidates.size(), false);
    for (std::size_t firstIndex = 0U; firstIndex < candidates.size(); ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1U; secondIndex < candidates.size();
             ++secondIndex) {
            if (sharesInitialMeasurementAtSameFrameIndex(candidates[firstIndex],
                                                         candidates[secondIndex], rule, space)) {
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

auto deduplicateInitialCandidatesByCentroid(std::vector<Core::TargetInfo> candidates,
                                            const InitialMeasurementDedupRule& rule)
    -> std::vector<Core::TargetInfo> {
    return deduplicateInitialCandidates(std::move(candidates), rule,
                                        CandidateMeasurementSpace::Centroid);
}

}  // namespace Dss::Tracking
