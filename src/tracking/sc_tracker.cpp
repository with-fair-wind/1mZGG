#include "dss/tracking/sc_tracker.h"

#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dss/tracking/candidate_utils.h"
#include "dss/tracking/lifecycle_utils.h"
#include "dss/tracking/prediction_utils.h"

namespace {

constexpr std::size_t kScAssocFrameCount = 3U;
constexpr float kScFovCenterHalfExtent = 128.0F;

using Dss::Tracking::aeMotion;
using Dss::Tracking::appendMatchedFrameAndUpdatePrediction;
using Dss::Tracking::BlobMatchOptions;
using Dss::Tracking::BlobMatchSpace;
using Dss::Tracking::deduplicateInitialCandidatesByCentroid;
using Dss::Tracking::frameMotion;
using Dss::Tracking::framePeriodSeconds;
using Dss::Tracking::InitialMeasurementDedupRule;
using Dss::Tracking::isNearFovCenter;
using Dss::Tracking::makeTargetFrameInfo;
using Dss::Tracking::targetRemainsLiving;
using Dss::Tracking::TrackLivingRule;
using Dss::Tracking::TrackMissPolicy;

[[nodiscard]] bool passesScAssocGate(const Dss::Core::Vec2f& firstFrameMotion,
                                     const Dss::Core::Vec2f& secondFrameMotion,
                                     const Dss::Core::MeasuredBlob& secondBlob,
                                     const Dss::Core::TrackingSettings& settings) {
    return std::abs(firstFrameMotion.x) < settings.searchRadius &&
           std::abs(firstFrameMotion.y) < settings.searchRadius &&
           isNearFovCenter(secondBlob, settings, kScFovCenterHalfExtent) &&
           std::abs(secondFrameMotion.x - firstFrameMotion.x) <= settings.thresholdMeo &&
           std::abs(secondFrameMotion.y - firstFrameMotion.y) <= settings.thresholdMeo;
}

[[nodiscard]] auto makeAssociatedTarget(
    const Dss::Core::FrameMeasurements& firstFrame, const Dss::Core::MeasuredBlob& firstBlob,
    const Dss::Core::FrameMeasurements& secondFrame, const Dss::Core::MeasuredBlob& secondBlob,
    const Dss::Core::FrameMeasurements& thirdFrame, const Dss::Core::MeasuredBlob& thirdBlob,
    const Dss::Core::TrackingSettings& settings, std::size_t targetIndex) -> Dss::Core::TargetInfo {
    const auto firstFrameMotion = frameMotion(firstBlob, secondBlob);
    const auto secondFrameMotion = frameMotion(secondBlob, thirdBlob);
    const auto firstAeMotion = aeMotion(firstBlob, secondBlob);
    const auto secondAeMotion = aeMotion(secondBlob, thirdBlob);
    const auto period = framePeriodSeconds(thirdFrame);

    Dss::Core::TargetInfo target{};
    target.targetId = "sc-" + std::to_string(targetIndex + 1U);
    target.frameInfos.push_back(makeTargetFrameInfo(firstFrame, firstBlob, settings));
    target.frameInfos.push_back(makeTargetFrameInfo(secondFrame, secondBlob, settings));
    target.frameInfos.push_back(makeTargetFrameInfo(thirdFrame, thirdBlob, settings));
    target.predictedSpdFrame = Dss::Core::Vec2f{(firstFrameMotion.x + secondFrameMotion.x) / 2.0F,
                                                (firstFrameMotion.y + secondFrameMotion.y) / 2.0F};
    target.predictedPosFrame = Dss::Core::Vec2f{thirdBlob.centroid.x + target.predictedSpdFrame.x,
                                                thirdBlob.centroid.y + target.predictedSpdFrame.y};
    target.predictedSpdAe = Dss::Core::Vec2f{(firstAeMotion.x + secondAeMotion.x) / period / 2.0F,
                                             (firstAeMotion.y + secondAeMotion.y) / period / 2.0F};
    target.predictedPosAe = Dss::Core::Vec2f{thirdBlob.posAe.x + target.predictedSpdAe.x * period,
                                             thirdBlob.posAe.y + target.predictedSpdAe.y * period};
    target.validity = 1.0F;
    target.living = true;
    return target;
}

[[nodiscard]] auto compressSimilarInitialCandidates(std::vector<Dss::Core::TargetInfo> candidates)
    -> std::vector<Dss::Core::TargetInfo> {
    InitialMeasurementDedupRule rule{};
    rule.frameCount = kScAssocFrameCount;
    return deduplicateInitialCandidatesByCentroid(std::move(candidates), rule);
}

/**
 * @brief 对最近三帧执行恒星校准目标关联
 *
 * 要求像面运动在搜索半径内、第二帧位于视场中心，且运动一致性满足 MEO 门限。
 */
[[nodiscard]] auto associateThreeFrameTargets(const std::deque<Dss::Core::FrameMeasurements>& fifo,
                                              const Dss::Core::TrackingSettings& settings)
    -> std::vector<Dss::Core::TargetInfo> {
    if (fifo.size() < kScAssocFrameCount) {
        return {};
    }

    const auto& firstFrame = fifo[fifo.size() - 3U];
    const auto& secondFrame = fifo[fifo.size() - 2U];
    const auto& thirdFrame = fifo[fifo.size() - 1U];
    if (firstFrame.targetBlobs.empty() || secondFrame.targetBlobs.empty() ||
        thirdFrame.targetBlobs.empty()) {
        return {};
    }

    std::vector<Dss::Core::TargetInfo> targets;
    for (const auto& firstBlob : firstFrame.targetBlobs) {
        for (const auto& secondBlob : secondFrame.targetBlobs) {
            const auto firstFrameMotion = frameMotion(firstBlob, secondBlob);
            for (const auto& thirdBlob : thirdFrame.targetBlobs) {
                const auto secondFrameMotion = frameMotion(secondBlob, thirdBlob);
                if (passesScAssocGate(firstFrameMotion, secondFrameMotion, secondBlob, settings)) {
                    targets.push_back(makeAssociatedTarget(firstFrame, firstBlob, secondFrame,
                                                           secondBlob, thirdFrame, thirdBlob,
                                                           settings, targets.size()));
                }
            }
        }
    }
    return compressSimilarInitialCandidates(std::move(targets));
}

[[nodiscard]] auto makeScFrameBlobMatchOptions(const Dss::Core::TrackingSettings& settings,
                                               bool requireFovCenter) -> BlobMatchOptions {
    BlobMatchOptions options{};
    options.space = BlobMatchSpace::Frame;
    options.threshold = settings.thresholdMeo;
    options.requireFovCenter = requireFovCenter;
    options.fovCenterHalfExtent = kScFovCenterHalfExtent;
    return options;
}

[[nodiscard]] auto makeScVerifyLivingRule(const Dss::Core::TrackingSettings& settings)
    -> TrackLivingRule {
    TrackLivingRule rule{};
    rule.frameWindow = settings.numFramesLiving;
    rule.threshold = settings.thresholdLiving;
    rule.missPolicy = TrackMissPolicy::UseValidityWindow;
    return rule;
}

[[nodiscard]] auto makeScTrackLivingRule(const Dss::Core::TrackingSettings& settings)
    -> TrackLivingRule {
    TrackLivingRule rule{};
    rule.frameWindow = settings.numFramesLiving;
    rule.threshold = settings.thresholdLiving;
    rule.missPolicy = TrackMissPolicy::RequireLatestValid;
    return rule;
}

/// 在当前帧验证各候选目标，按有效性窗口规则判定存活
[[nodiscard]] auto verifyTargetsOnFrame(const Dss::Core::FrameMeasurements& frame,
                                        const std::vector<Dss::Core::TargetInfo>& candidates,
                                        const Dss::Core::TrackingSettings& settings)
    -> std::vector<Dss::Core::TargetInfo> {
    std::vector<Dss::Core::TargetInfo> verifiedTargets;
    const auto matchOptions = makeScFrameBlobMatchOptions(settings, false);
    for (const auto& candidate : candidates) {
        if (!candidate.living) {
            continue;
        }
        auto verified =
            appendMatchedFrameAndUpdatePrediction(frame, candidate, settings, matchOptions);
        verified.living = targetRemainsLiving(verified, makeScVerifyLivingRule(settings));
        if (verified.living) {
            verifiedTargets.push_back(verified);
        }
    }
    return verifiedTargets;
}

/// 从验证通过的候选中选择最新帧像斑面积最大的目标
[[nodiscard]] auto selectLargestLatestAreaTarget(const std::vector<Dss::Core::TargetInfo>& targets)
    -> std::optional<Dss::Core::TargetInfo> {
    const Dss::Core::TargetInfo* selectedTarget = nullptr;
    auto selectedArea = 0.0F;
    for (const auto& target : targets) {
        if (!target.living || target.frameInfos.empty()) {
            continue;
        }
        const auto& latestFrame = target.frameInfos.back();
        if (!latestFrame.valid || latestFrame.measuredBlob.area <= selectedArea) {
            continue;
        }
        selectedTarget = &target;
        selectedArea = latestFrame.measuredBlob.area;
    }

    if (selectedTarget == nullptr) {
        return std::nullopt;
    }
    return *selectedTarget;
}

/// 对选定目标执行单帧像面空间匹配，要求候选位于视场中心
[[nodiscard]] auto trackTargetOnFrame(const Dss::Core::FrameMeasurements& frame,
                                      const Dss::Core::TargetInfo& target,
                                      const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetInfo {
    auto tracked = target;
    if (!tracked.living) {
        return tracked;
    }

    tracked = appendMatchedFrameAndUpdatePrediction(frame, tracked, settings,
                                                    makeScFrameBlobMatchOptions(settings, true));
    tracked.living = targetRemainsLiving(tracked, makeScTrackLivingRule(settings));
    return tracked;
}

}  // namespace

namespace Dss::Tracking {

ScTracker::ScTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

/// 恒星校准跟踪主流程：三帧关联 → 验证 → 选最大面积目标 → 持续跟踪
auto ScTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifo.push_back(measurements);
    if (m_fifo.size() > kScAssocFrameCount) {
        m_fifo.pop_front();
    }

    if (!m_targetFound) {
        m_candidates = associateThreeFrameTargets(m_fifo, m_settings);
        m_targetFound = !m_candidates.empty();
        m_targetVerified = false;
        if (!m_targetFound) {
            return {};
        }
        m_currentTarget = m_candidates.front();
        return m_candidates;
    }

    if (!m_targetVerified) {
        const auto verifiedTargets = verifyTargetsOnFrame(measurements, m_candidates, m_settings);
        const auto selectedTarget = selectLargestLatestAreaTarget(verifiedTargets);
        if (!selectedTarget.has_value()) {
            m_currentTarget = {};
            m_candidates.clear();
            m_targetFound = false;
            m_targetVerified = false;
            return {};
        }
        m_candidates = verifiedTargets;
        m_currentTarget = *selectedTarget;
        m_targetVerified = true;
        return {m_currentTarget};
    }

    m_currentTarget = trackTargetOnFrame(measurements, m_currentTarget, m_settings);
    if (!m_currentTarget.living) {
        m_currentTarget = {};
        m_candidates.clear();
        m_targetFound = false;
        m_targetVerified = false;
        return {};
    }
    return {m_currentTarget};
}

void ScTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
    m_candidates.clear();
    m_targetFound = false;
    m_targetVerified = false;
}

}  // namespace Dss::Tracking
