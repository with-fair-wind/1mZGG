#include "dss/tracking/sc_tracker.h"

#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "dss/tracking/prediction_utils.h"

namespace {

constexpr std::size_t kScAssocFrameCount = 3U;
constexpr float kScFovCenterHalfExtent = 128.0F;

using Dss::Tracking::aeMotion;
using Dss::Tracking::BlobMatchOptions;
using Dss::Tracking::BlobMatchSpace;
using Dss::Tracking::findNearestBlob;
using Dss::Tracking::frameMotion;
using Dss::Tracking::framePeriodSeconds;
using Dss::Tracking::isNearFovCenter;
using Dss::Tracking::makeInvalidTargetFrameInfo;
using Dss::Tracking::makeTargetFrameInfo;
using Dss::Tracking::updatePredictionFromRecentFour;

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

[[nodiscard]] bool hasSameInitialMeasurement(const Dss::Core::TargetInfo& first,
                                             const Dss::Core::TargetInfo& second,
                                             std::size_t frameIndex) {
    if (first.frameInfos.size() <= frameIndex || second.frameInfos.size() <= frameIndex) {
        return false;
    }

    const auto& firstCentroid = first.frameInfos[frameIndex].measuredBlob.centroid;
    const auto& secondCentroid = second.frameInfos[frameIndex].measuredBlob.centroid;
    return firstCentroid.x == secondCentroid.x && firstCentroid.y == secondCentroid.y;
}

[[nodiscard]] bool reusesAnyInitialMeasurement(
    const Dss::Core::TargetInfo& candidate, const std::vector<Dss::Core::TargetInfo>& keptTargets) {
    for (const auto& keptTarget : keptTargets) {
        for (std::size_t frameIndex = 0U; frameIndex < kScAssocFrameCount; ++frameIndex) {
            if (hasSameInitialMeasurement(candidate, keptTarget, frameIndex)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] auto compressSimilarInitialCandidates(
    const std::vector<Dss::Core::TargetInfo>& candidates) -> std::vector<Dss::Core::TargetInfo> {
    std::vector<Dss::Core::TargetInfo> compressed;
    compressed.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (!reusesAnyInitialMeasurement(candidate, compressed)) {
            compressed.push_back(candidate);
        }
    }
    return compressed;
}

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
    return compressSimilarInitialCandidates(targets);
}

[[nodiscard]] auto findNearestFrameBlob(const Dss::Core::FrameMeasurements& frame,
                                        const Dss::Core::TargetInfo& target,
                                        const Dss::Core::TrackingSettings& settings,
                                        bool requireFovCenter) -> const Dss::Core::MeasuredBlob* {
    BlobMatchOptions options{};
    options.space = BlobMatchSpace::Frame;
    options.threshold = settings.thresholdMeo;
    options.requireFovCenter = requireFovCenter;
    options.fovCenterHalfExtent = kScFovCenterHalfExtent;
    return findNearestBlob(frame, target, settings, options);
}

[[nodiscard]] bool passesScVerifyLivingRule(const Dss::Core::TargetInfo& target,
                                            const Dss::Core::TrackingSettings& settings) {
    if (target.frameInfos.empty()) {
        return false;
    }
    const auto livingWindow =
        settings.numFramesLiving > 0 ? static_cast<std::size_t>(settings.numFramesLiving) : 0U;
    return !(livingWindow > 0U && target.frameInfos.size() >= livingWindow &&
             target.validity < settings.thresholdLiving && !target.frameInfos.back().valid);
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
        const auto* matchedBlob = findNearestFrameBlob(frame, candidate, settings, false);
        auto verified = candidate;
        verified.frameInfos.push_back(matchedBlob == nullptr
                                          ? makeInvalidTargetFrameInfo(frame, verified, settings)
                                          : makeTargetFrameInfo(frame, *matchedBlob, settings));
        updatePredictionFromRecentFour(verified);
        verified.living = passesScVerifyLivingRule(verified, settings);
        if (verified.living) {
            verifiedTargets.push_back(verified);
        }
    }
    return verifiedTargets;
}

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

[[nodiscard]] auto trackTargetOnFrame(const Dss::Core::FrameMeasurements& frame,
                                      const Dss::Core::TargetInfo& target,
                                      const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetInfo {
    auto tracked = target;
    if (!tracked.living) {
        return tracked;
    }

    const auto* matchedBlob = findNearestFrameBlob(frame, tracked, settings, true);
    tracked.frameInfos.push_back(matchedBlob == nullptr
                                     ? makeInvalidTargetFrameInfo(frame, tracked, settings)
                                     : makeTargetFrameInfo(frame, *matchedBlob, settings));
    updatePredictionFromRecentFour(tracked);
    tracked.living = tracked.frameInfos.back().valid;
    return tracked;
}

}  // namespace

namespace Dss::Tracking {

ScTracker::ScTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

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
