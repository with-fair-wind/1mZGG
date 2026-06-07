#include "dss/tracking/sc_tracker.h"

#include <cmath>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kScAssocFrameCount = 3U;
constexpr float kScFovCenterHalfExtent = 128.0F;

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

[[nodiscard]] auto framePeriodSeconds(const Dss::Core::FrameMeasurements& frame) -> float {
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

[[nodiscard]] bool isNearFovCenter(const Dss::Core::MeasuredBlob& blob,
                                   const Dss::Core::TrackingSettings& settings) {
    return std::abs(blob.centroid.x - settings.opticParams.fovCenterX) < kScFovCenterHalfExtent &&
           std::abs(blob.centroid.y - settings.opticParams.fovCenterY) < kScFovCenterHalfExtent;
}

[[nodiscard]] bool passesScAssocGate(const Dss::Core::Vec2f& firstFrameMotion,
                                     const Dss::Core::Vec2f& secondFrameMotion,
                                     const Dss::Core::MeasuredBlob& secondBlob,
                                     const Dss::Core::TrackingSettings& settings) {
    return std::abs(firstFrameMotion.x) < settings.searchRadius &&
           std::abs(firstFrameMotion.y) < settings.searchRadius &&
           isNearFovCenter(secondBlob, settings) &&
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
    return targets;
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
        if (!m_targetFound) {
            return {};
        }
        m_currentTarget = m_candidates.front();
        return m_candidates;
    }

    return {m_currentTarget};
}

void ScTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
    m_candidates.clear();
    m_targetFound = false;
}

}  // namespace Dss::Tracking
