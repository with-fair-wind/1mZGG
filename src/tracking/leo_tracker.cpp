#include "dss/tracking/leo_tracker.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kLeoAssocFrameCount = 3U;
constexpr std::size_t kMaxLeoInitialBlobCount = 2000U;
constexpr std::size_t kLeoTrackInvalidLivingFrameCount = 5U;
constexpr float kInvalidBlobHalfExtent = 5.0F;

[[nodiscard]] auto median3(float first, float second, float third) -> float {
    const auto minValue = std::min(first, std::min(second, third));
    const auto maxValue = std::max(first, std::max(second, third));
    return first + second + third - minValue - maxValue;
}

[[nodiscard]] auto makeFrameInfo(const Dss::Core::FrameMeasurements& frame,
                                 const Dss::Core::MeasuredBlob& blob,
                                 const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetFrameInfo {
    Dss::Core::TargetFrameInfo info{};
    info.timestamp = frame.timestamp;
    info.frameSeq = frame.frameSeq;
    info.fovCenterAe = frame.fovCenterAe;
    info.opticCenter =
        Dss::Core::Vec2f{settings.opticParams.fovCenterX, settings.opticParams.fovCenterY};
    info.exposureTime = frame.exposureTime;
    info.frameFreq = frame.frameFreq;
    info.measuredBlob = blob;
    info.posZxdw = blob.posAe;
    info.posTwdw = blob.posAe;
    info.valid = true;
    return info;
}

[[nodiscard]] auto makeInvalidFrameInfo(const Dss::Core::FrameMeasurements& frame,
                                        const Dss::Core::TargetInfo& target,
                                        const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetFrameInfo {
    Dss::Core::MeasuredBlob predictedBlob{};
    predictedBlob.id = target.targetId;
    predictedBlob.centroid = target.predictedPosFrame;
    predictedBlob.maxX = predictedBlob.centroid.x + kInvalidBlobHalfExtent;
    predictedBlob.minX = predictedBlob.centroid.x - kInvalidBlobHalfExtent;
    predictedBlob.maxY = predictedBlob.centroid.y + kInvalidBlobHalfExtent;
    predictedBlob.minY = predictedBlob.centroid.y - kInvalidBlobHalfExtent;
    predictedBlob.area = 0.0F;
    predictedBlob.dn = 0.0F;
    predictedBlob.posAe = target.predictedPosAe;

    auto info = makeFrameInfo(frame, predictedBlob, settings);
    info.valid = false;
    return info;
}

[[nodiscard]] auto framePeriodSeconds(const Dss::Core::FrameMeasurements& frame) -> float {
    return frame.frameFreq > 0.0F ? 1.0F / frame.frameFreq : 1.0F;
}

[[nodiscard]] auto framePeriodSecondsFromFrameInfo(const Dss::Core::TargetFrameInfo& frame)
    -> float {
    return frame.frameFreq > 0.0F ? 1.0F / frame.frameFreq : 1.0F;
}

[[nodiscard]] auto frameMotion(const Dss::Core::MeasuredBlob& from,
                               const Dss::Core::MeasuredBlob& to) -> Dss::Core::Vec2f {
    return Dss::Core::Vec2f{to.centroid.x - from.centroid.x, to.centroid.y - from.centroid.y};
}

[[nodiscard]] auto aeMotion(const Dss::Core::MeasuredBlob& from, const Dss::Core::MeasuredBlob& to)
    -> Dss::Core::Vec2f {
    return Dss::Core::Vec2f{to.posAe.x - from.posAe.x, to.posAe.y - from.posAe.y};
}

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
    target.frameInfos.push_back(makeFrameInfo(firstFrame, firstBlob, settings));
    target.frameInfos.push_back(makeFrameInfo(secondFrame, secondBlob, settings));
    target.frameInfos.push_back(makeFrameInfo(thirdFrame, thirdBlob, settings));
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

[[nodiscard]] auto findNearestVerificationBlob(const Dss::Core::FrameMeasurements& frame,
                                               const Dss::Core::TargetInfo& target,
                                               float thresholdAe)
    -> const Dss::Core::MeasuredBlob* {
    const Dss::Core::MeasuredBlob* nearestBlob = nullptr;
    auto nearestDistance = std::numeric_limits<float>::max();
    for (const auto& blob : frame.targetBlobs) {
        const auto dx = target.predictedPosAe.x - blob.posAe.x;
        const auto dy = target.predictedPosAe.y - blob.posAe.y;
        const auto distance = static_cast<float>(std::hypot(dx, dy));
        if (std::abs(dx) < thresholdAe && std::abs(dy) < thresholdAe &&
            distance <= nearestDistance) {
            nearestBlob = &blob;
            nearestDistance = distance;
        }
    }
    return nearestBlob;
}

[[nodiscard]] auto targetFrameMotionAt(const Dss::Core::TargetInfo& target, std::size_t index)
    -> Dss::Core::Vec2f {
    const auto& current = target.frameInfos[index].measuredBlob;
    const auto& previous = target.frameInfos[index - 1U].measuredBlob;
    return frameMotion(previous, current);
}

[[nodiscard]] auto targetAeMotionAt(const Dss::Core::TargetInfo& target, std::size_t index)
    -> Dss::Core::Vec2f {
    const auto& current = target.frameInfos[index].measuredBlob;
    const auto& previous = target.frameInfos[index - 1U].measuredBlob;
    return aeMotion(previous, current);
}

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

[[nodiscard]] bool hasConsecutiveInvalidFrames(const Dss::Core::TargetInfo& target,
                                               std::size_t frameCount) {
    if (target.frameInfos.size() < frameCount) {
        return false;
    }

    const auto firstRecentIndex = target.frameInfos.size() - frameCount;
    for (auto index = firstRecentIndex; index < target.frameInfos.size(); ++index) {
        if (target.frameInfos[index].valid) {
            return false;
        }
    }
    return true;
}

void updatePredictionFromRecentFour(Dss::Core::TargetInfo& target) {
    const auto size = target.frameInfos.size();
    if (size < 4U) {
        return;
    }

    const auto frameMotion3 = targetFrameMotionAt(target, size - 1U);
    const auto frameMotion2 = targetFrameMotionAt(target, size - 2U);
    const auto frameMotion1 = targetFrameMotionAt(target, size - 3U);
    const auto aeMotion3 = targetAeMotionAt(target, size - 1U);
    const auto aeMotion2 = targetAeMotionAt(target, size - 2U);
    const auto aeMotion1 = targetAeMotionAt(target, size - 3U);
    const auto& latestBlob = target.frameInfos.back().measuredBlob;
    const auto period = framePeriodSecondsFromFrameInfo(target.frameInfos.back());

    target.predictedSpdFrame =
        Dss::Core::Vec2f{median3(frameMotion3.x, frameMotion2.x, frameMotion1.x),
                         median3(frameMotion3.y, frameMotion2.y, frameMotion1.y)};
    target.predictedPosFrame = Dss::Core::Vec2f{latestBlob.centroid.x + target.predictedSpdFrame.x,
                                                latestBlob.centroid.y + target.predictedSpdFrame.y};
    target.predictedSpdAe =
        Dss::Core::Vec2f{median3(aeMotion3.x, aeMotion2.x, aeMotion1.x) / period,
                         median3(aeMotion3.y, aeMotion2.y, aeMotion1.y) / period};
    target.predictedPosAe = Dss::Core::Vec2f{latestBlob.posAe.x + target.predictedSpdAe.x * period,
                                             latestBlob.posAe.y + target.predictedSpdAe.y * period};

    const auto latestValid = target.frameInfos.back().valid ? 1.0F : 0.0F;
    target.validity = ((static_cast<float>(size - 1U) * target.validity) + latestValid) /
                      static_cast<float>(size);
}

[[nodiscard]] auto aeSpeedMagnitude(const Dss::Core::TargetInfo& target) -> float {
    return static_cast<float>(std::hypot(target.predictedSpdAe.x, target.predictedSpdAe.y));
}

[[nodiscard]] auto verifyTargetsOnFrame(const Dss::Core::FrameMeasurements& frame,
                                        const std::vector<Dss::Core::TargetInfo>& candidates,
                                        const Dss::Core::TrackingSettings& settings)
    -> std::vector<Dss::Core::TargetInfo> {
    std::vector<Dss::Core::TargetInfo> verifiedTargets;
    for (const auto& candidate : candidates) {
        if (!candidate.living) {
            continue;
        }
        const auto* matchedBlob =
            findNearestVerificationBlob(frame, candidate, settings.thresholdAe);
        auto verified = candidate;
        verified.frameInfos.push_back(matchedBlob == nullptr
                                          ? makeInvalidFrameInfo(frame, verified, settings)
                                          : makeFrameInfo(frame, *matchedBlob, settings));
        updatePredictionFromRecentFour(verified);
        verified.living = hasConsistentRecentAeMotion(verified, settings.thresholdAe);
        if (verified.living && verified.frameInfos.back().valid) {
            verifiedTargets.push_back(verified);
        }
    }
    return verifiedTargets;
}

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

[[nodiscard]] auto trackTargetOnFrame(const Dss::Core::FrameMeasurements& frame,
                                      const Dss::Core::TargetInfo& target,
                                      const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetInfo {
    auto tracked = target;
    if (!tracked.living) {
        return tracked;
    }

    const auto* matchedBlob = findNearestVerificationBlob(frame, tracked, settings.thresholdAe);
    tracked.frameInfos.push_back(matchedBlob == nullptr
                                     ? makeInvalidFrameInfo(frame, tracked, settings)
                                     : makeFrameInfo(frame, *matchedBlob, settings));
    updatePredictionFromRecentFour(tracked);
    tracked.living =
        tracked.living && !hasConsecutiveInvalidFrames(tracked, kLeoTrackInvalidLivingFrameCount);
    return tracked;
}

}  // namespace

namespace Dss::Tracking {

LeoTracker::LeoTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

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
