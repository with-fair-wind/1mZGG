#include "dss/tracking/leo_tracker.h"

#include <cmath>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "dss/tracking/lifecycle_utils.h"
#include "dss/tracking/prediction_utils.h"

namespace {

constexpr std::size_t kLeoAssocFrameCount = 3U;
constexpr std::size_t kMaxLeoInitialBlobCount = 2000U;
constexpr int kLeoTrackInvalidLivingFrameCount = 5;

using Dss::Tracking::aeMotion;
using Dss::Tracking::BlobMatchOptions;
using Dss::Tracking::BlobMatchSpace;
using Dss::Tracking::findNearestBlob;
using Dss::Tracking::frameMotion;
using Dss::Tracking::framePeriodSeconds;
using Dss::Tracking::makeInvalidTargetFrameInfo;
using Dss::Tracking::makeTargetFrameInfo;
using Dss::Tracking::targetAeMotionAt;
using Dss::Tracking::targetRemainsLiving;
using Dss::Tracking::TrackLivingRule;
using Dss::Tracking::TrackMissPolicy;
using Dss::Tracking::updatePredictionFromRecentFour;

[[nodiscard]] bool passesLeoAssocGate(const Dss::Core::Vec2f& firstAeMotion,
                                      const Dss::Core::Vec2f& secondAeMotion,
                                      const Dss::Core::TrackingSettings& settings) {
    const auto firstSpeed = static_cast<float>(std::hypot(firstAeMotion.x, firstAeMotion.y));
    return firstSpeed > settings.spdLowAe &&
           std::abs(secondAeMotion.x - firstAeMotion.x) <= settings.thresholdAe &&
           std::abs(secondAeMotion.y - firstAeMotion.y) <= settings.thresholdAe;
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
    target.targetId = "leo-" + std::to_string(targetIndex + 1U);
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

/**
 * @brief 对最近三帧执行 LEO 目标关联
 *
 * 要求相邻帧 AE 运动满足速度下限与一致性门限，生成初始候选列表。
 */
[[nodiscard]] auto associateThreeFrameTargets(const std::deque<Dss::Core::FrameMeasurements>& fifo,
                                              const Dss::Core::TrackingSettings& settings)
    -> std::vector<Dss::Core::TargetInfo> {
    if (fifo.size() < kLeoAssocFrameCount) {
        return {};
    }

    const auto& firstFrame = fifo[fifo.size() - 3U];
    const auto& secondFrame = fifo[fifo.size() - 2U];
    const auto& thirdFrame = fifo[fifo.size() - 1U];
    if (firstFrame.targetBlobs.empty() || secondFrame.targetBlobs.empty() ||
        thirdFrame.targetBlobs.empty() ||
        thirdFrame.targetBlobs.size() >= kMaxLeoInitialBlobCount) {
        return {};
    }

    std::vector<Dss::Core::TargetInfo> targets;
    for (const auto& firstBlob : firstFrame.targetBlobs) {
        for (const auto& secondBlob : secondFrame.targetBlobs) {
            const auto firstAeMotion = aeMotion(firstBlob, secondBlob);
            for (const auto& thirdBlob : thirdFrame.targetBlobs) {
                const auto secondAeMotion = aeMotion(secondBlob, thirdBlob);
                if (passesLeoAssocGate(firstAeMotion, secondAeMotion, settings)) {
                    targets.push_back(makeAssociatedTarget(firstFrame, firstBlob, secondFrame,
                                                           secondBlob, thirdFrame, thirdBlob,
                                                           settings, targets.size()));
                }
            }
        }
    }
    return targets;
}

[[nodiscard]] auto findNearestAeBlob(const Dss::Core::FrameMeasurements& frame,
                                     const Dss::Core::TargetInfo& target,
                                     const Dss::Core::TrackingSettings& settings)
    -> const Dss::Core::MeasuredBlob* {
    BlobMatchOptions options{};
    options.space = BlobMatchSpace::Ae;
    options.threshold = settings.thresholdAe;
    return findNearestBlob(frame, target, settings, options);
}

/// 检查最近四帧 AE 运动的一致性，用于 LEO 目标验证
[[nodiscard]] bool hasConsistentRecentAeMotion(const Dss::Core::TargetInfo& target,
                                               float thresholdAe) {
    if (target.frameInfos.size() < 4U) {
        return false;
    }
    const auto size = target.frameInfos.size();
    const auto latestValid =
        target.frameInfos[size - 1U].valid && target.frameInfos[size - 2U].valid &&
        target.frameInfos[size - 3U].valid && target.frameInfos[size - 4U].valid;
    if (!latestValid) {
        return false;
    }

    const auto motion3 = targetAeMotionAt(target, size - 1U);
    const auto motion2 = targetAeMotionAt(target, size - 2U);
    const auto motion1 = targetAeMotionAt(target, size - 3U);
    const auto validCount = (std::abs(motion2.x - motion1.x) < thresholdAe ? 1 : 0) +
                            (std::abs(motion3.x - motion2.x) < thresholdAe ? 1 : 0) +
                            (std::abs(motion3.x - motion1.x) < thresholdAe ? 1 : 0) +
                            (std::abs(motion2.y - motion1.y) < thresholdAe ? 1 : 0) +
                            (std::abs(motion3.y - motion2.y) < thresholdAe ? 1 : 0) +
                            (std::abs(motion3.y - motion1.y) < thresholdAe ? 1 : 0);
    return validCount >= 6;
}

[[nodiscard]] auto aeSpeedMagnitude(const Dss::Core::TargetInfo& target) -> float {
    return static_cast<float>(std::hypot(target.predictedSpdAe.x, target.predictedSpdAe.y));
}

/// 在当前帧验证各候选目标，保留 AE 运动一致且最新帧有效的候选
[[nodiscard]] auto verifyTargetsOnFrame(const Dss::Core::FrameMeasurements& frame,
                                        const std::vector<Dss::Core::TargetInfo>& candidates,
                                        const Dss::Core::TrackingSettings& settings)
    -> std::vector<Dss::Core::TargetInfo> {
    std::vector<Dss::Core::TargetInfo> verifiedTargets;
    for (const auto& candidate : candidates) {
        if (!candidate.living) {
            continue;
        }
        const auto* matchedBlob = findNearestAeBlob(frame, candidate, settings);
        auto verified = candidate;
        verified.frameInfos.push_back(matchedBlob == nullptr
                                          ? makeInvalidTargetFrameInfo(frame, verified, settings)
                                          : makeTargetFrameInfo(frame, *matchedBlob, settings));
        updatePredictionFromRecentFour(verified);
        verified.living = hasConsistentRecentAeMotion(verified, settings.thresholdAe);
        if (verified.living && verified.frameInfos.back().valid) {
            verifiedTargets.push_back(verified);
        }
    }
    return verifiedTargets;
}

/// 从验证通过的候选中选择 AE 速度最大的目标
[[nodiscard]] auto selectFastestAeTarget(const std::vector<Dss::Core::TargetInfo>& targets)
    -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo selected{};
    auto selectedSpeed = 0.0F;
    for (const auto& target : targets) {
        const auto speed = aeSpeedMagnitude(target);
        if (speed > selectedSpeed) {
            selected = target;
            selectedSpeed = speed;
        }
    }
    return selected;
}

[[nodiscard]] auto makeLeoTrackLivingRule() -> TrackLivingRule {
    TrackLivingRule rule{};
    rule.frameWindow = kLeoTrackInvalidLivingFrameCount;
    rule.missPolicy = TrackMissPolicy::DropAfterConsecutiveInvalidFrames;
    return rule;
}

/// 对选定目标执行单帧 AE 空间匹配并更新存活状态
[[nodiscard]] auto trackTargetOnFrame(const Dss::Core::FrameMeasurements& frame,
                                      const Dss::Core::TargetInfo& target,
                                      const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetInfo {
    auto tracked = target;
    if (!tracked.living) {
        return tracked;
    }

    const auto* matchedBlob = findNearestAeBlob(frame, tracked, settings);
    tracked.frameInfos.push_back(matchedBlob == nullptr
                                     ? makeInvalidTargetFrameInfo(frame, tracked, settings)
                                     : makeTargetFrameInfo(frame, *matchedBlob, settings));
    updatePredictionFromRecentFour(tracked);
    tracked.living = tracked.living && targetRemainsLiving(tracked, makeLeoTrackLivingRule());
    return tracked;
}

}  // namespace

namespace Dss::Tracking {

LeoTracker::LeoTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

/// LEO 跟踪主流程：三帧关联 → 验证 → 选定最快目标 → 持续跟踪
auto LeoTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifo.push_back(measurements);
    if (m_fifo.size() > kLeoAssocFrameCount) {
        m_fifo.pop_front();
    }

    if (!m_targetFound) {
        m_candidates = associateThreeFrameTargets(m_fifo, m_settings);
        m_targetFound = !m_candidates.empty();
        if (!m_targetFound) {
            return {};
        }
        m_currentTarget = m_candidates.front();
        return m_candidates;
    }

    if (!m_targetVerified) {
        const auto verifiedTargets = verifyTargetsOnFrame(measurements, m_candidates, m_settings);
        if (verifiedTargets.empty()) {
            m_currentTarget = {};
            m_candidates.clear();
            m_targetFound = false;
            return {};
        }
        m_candidates = verifiedTargets;
        m_currentTarget = selectFastestAeTarget(m_candidates);
        m_targetVerified = true;
        return {m_currentTarget};
    }

    m_currentTarget = trackTargetOnFrame(measurements, m_currentTarget, m_settings);
    if (!m_currentTarget.living) {
        m_candidates.clear();
        m_targetFound = false;
        m_targetVerified = false;
        return {};
    }
    return {m_currentTarget};
}

void LeoTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
    m_candidates.clear();
    m_targetFound = false;
    m_targetVerified = false;
}

}  // namespace Dss::Tracking
