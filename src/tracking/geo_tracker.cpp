#include "dss/tracking/geo_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "dss/tracking/candidate_utils.h"
#include "dss/tracking/lifecycle_utils.h"
#include "dss/tracking/prediction_utils.h"
#include "geo_tracking_detail.h"

namespace Dss::Tracking {

namespace {

inline constexpr int kReliableStarMatchCount = 5;
inline constexpr std::size_t kMaxGeoRediscoveryBlobCount = 20000U;

using GeoDetail::associateFourFrameTargets;
using GeoDetail::candidateMeasurementSpace;
using GeoDetail::estimateGeoStarSpeedFromPair;
using GeoDetail::exceedsRediscoverySpeedLimit;
using GeoDetail::findNearestTrackedBlob;
using GeoDetail::framePeriodSeconds;
using GeoDetail::geoTrackingSpace;
using GeoDetail::GeoTrackingSpace;
using GeoDetail::hasLivingTarget;
using GeoDetail::isInsideTrackingBounds;
using GeoDetail::latestValidMeasurementsRepeatEquatorialPoint;
using GeoDetail::makeFrameInfo;
using GeoDetail::makeGeoTargetId;
using GeoDetail::updatePredictionFromHistory;

using Dss::Tracking::InvalidFallbackBlobOptions;
using Dss::Tracking::latestFramesAreAllInvalid;
using Dss::Tracking::makeInvalidFallbackBlob;
using Dss::Tracking::overlapsAnyLivingTarget;
using Dss::Tracking::RecentFrameWindowMode;
using Dss::Tracking::RecentMeasurementOverlapRule;
using Dss::Tracking::updateValidityWithLatestFrame;

}  // namespace
/**
 * @brief 使用帧序列末尾两帧估算 GEO 背景恒星速度。
 */
auto estimateGeoStarSpeed(std::span<const Dss::Core::FrameMeasurements> frames, float ratioFov,
                          float radius, const Dss::Core::OpticParams& opticParams)
    -> GeoStarSpeedResult {
    if (frames.size() < 2U) {
        return GeoStarSpeedResult{GeoStarSpeedStatus::InsufficientFrames};
    }
    return GeoDetail::estimateGeoStarSpeedFromPair(frames[frames.size() - 2U], frames.back(),
                                                   ratioFov, radius, opticParams);
}

GeoTracker::GeoTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

/**
 * @brief 执行单帧 GEO 跟踪主流程。
 */
auto GeoTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifoTarget.push_back(measurements);
    m_fifoStar.push_back(measurements);
    if (m_fifoTarget.size() > 10) {
        m_fifoTarget.pop_front();
    }
    if (m_fifoStar.size() > 5) {
        m_fifoStar.pop_front();
    }

    m_frameSeq = measurements.frameSeq;

    calcStarSpeed();

    if (!m_targetFound) {
        m_targets.clear();
        assoc4();
        findTargets();
    } else {
        trackTargets();
        if (measurements.targetBlobs.size() < kMaxGeoRediscoveryBlobCount) {
            refindTargets();
        }
        m_targetFound = hasLivingTarget(m_targets);
        m_targetVerified = m_targetFound;
    }

    return m_targets;
}

void GeoTracker::reset() {
    m_fifoTarget.clear();
    m_fifoStar.clear();
    m_targets.clear();
    m_starSpeed = {};
    m_starSpeedAe = {};
    m_frameFreq = 1.0F;
    m_starMatchCount = 0;
    m_targetFound = false;
    m_targetVerified = false;
    m_frameSeq = 0;
    m_nextTargetId = 1;
}

void GeoTracker::assignTargetIds(std::vector<Dss::Core::TargetInfo>& targets) {
    for (auto& target : targets) {
        if (!target.targetId.empty() && target.targetId != "geo") {
            continue;
        }
        target.targetId = makeGeoTargetId(m_nextTargetId);
        ++m_nextTargetId;
    }
}

/**
 * @brief 在匹配恒星数量足够时更新缓存的背景恒星速度。
 */
int GeoTracker::calcStarSpeed() {
    if (m_fifoStar.size() < 2U) {
        return 1;
    }

    const auto result = estimateGeoStarSpeedFromPair(
        m_fifoStar[m_fifoStar.size() - 2U], m_fifoStar.back(), m_settings.ratioFov,
        m_settings.searchRadius, m_settings.opticParams);
    if (result.status != GeoStarSpeedStatus::Ok) {
        return result.status == GeoStarSpeedStatus::MissingStars ? 2 : 1;
    }

    m_frameFreq = static_cast<float>(
        1.0 / framePeriodSeconds(m_fifoStar[m_fifoStar.size() - 2U], m_fifoStar.back()));
    m_starMatchCount = result.matchCount;
    if (result.matchCount >= kReliableStarMatchCount) {
        m_starSpeed = result.frameSpeed;
        m_starSpeedAe = result.aeSpeed;
    }
    return 0;
}

/**
 * @brief 对目标 FIFO 执行四帧关联并分配目标 ID。
 */
int GeoTracker::assoc4() {
    if (m_fifoTarget.size() < 4U) {
        return 1;
    }

    const auto frameCount = m_fifoTarget.size();
    const auto& frame0 = m_fifoTarget[frameCount - 4U];
    const auto& frame1 = m_fifoTarget[frameCount - 3U];
    const auto& frame2 = m_fifoTarget[frameCount - 2U];
    const auto& frame3 = m_fifoTarget[frameCount - 1U];
    if (frame0.targetBlobs.empty() || frame1.targetBlobs.empty() || frame2.targetBlobs.empty() ||
        frame3.targetBlobs.empty()) {
        return 2;
    }

    m_targets = associateFourFrameTargets(m_fifoTarget, m_starMatchCount, m_starSpeed, m_frameFreq,
                                          m_settings);
    assignTargetIds(m_targets);
    return 0;
}

/**
 * @brief 根据关联结果更新 GEO 目标发现状态。
 */
int GeoTracker::findTargets() {
    m_targetFound = !m_targets.empty();
    m_targetVerified = m_targetFound;
    return m_targetFound ? 0 : 1;
}

/**
 * @brief 在跟踪阶段重新执行四帧关联，并追加有效的重发现候选。
 */
int GeoTracker::refindTargets() {
    if (m_fifoTarget.size() < 5U) {
        return 1;
    }

    auto candidates = associateFourFrameTargets(m_fifoTarget, m_starMatchCount, m_starSpeed,
                                                m_frameFreq, m_settings);
    RecentMeasurementOverlapRule overlapRule{};
    overlapRule.frameCount = 4U;
    overlapRule.space = candidateMeasurementSpace(geoTrackingSpace(m_settings));
    std::erase_if(candidates, [&](const Dss::Core::TargetInfo& candidate) {
        return overlapsAnyLivingTarget(candidate, m_targets, overlapRule) ||
               exceedsRediscoverySpeedLimit(candidate) ||
               !isInsideTrackingBounds(candidate.predictedPosFrame, m_settings);
    });

    if (candidates.empty()) {
        return 2;
    }

    assignTargetIds(candidates);
    m_targets.insert(m_targets.end(), candidates.begin(), candidates.end());
    return 0;
}

/**
 * @brief
 * 将活跃目标与当前帧像斑匹配，并刷新预测、有效性与存活状态。

 */
int GeoTracker::trackTargets() {
    if (m_targets.empty() || m_fifoTarget.empty()) {
        return 1;
    }

    const auto& frame = m_fifoTarget.back();
    std::vector<Dss::Core::MeasuredBlob> usedBlobs;
    for (auto& target : m_targets) {
        if (!target.living) {
            continue;
        }

        const auto matchedBlob =
            findNearestTrackedBlob(frame.targetBlobs, target, m_settings, usedBlobs);
        if (!matchedBlob.has_value()) {
            InvalidFallbackBlobOptions fallbackOptions{};
            fallbackOptions.copyPredictedFrameToRaDec =
                geoTrackingSpace(m_settings) == GeoTrackingSpace::RaDec;
            auto info = makeFrameInfo(
                frame, makeInvalidFallbackBlob(frame, target, fallbackOptions), m_settings);
            info.valid = false;
            target.frameInfos.push_back(info);
        } else {
            target.frameInfos.push_back(makeFrameInfo(frame, *matchedBlob, m_settings));
            usedBlobs.push_back(*matchedBlob);
        }

        updatePredictionFromHistory(target, m_frameFreq, m_settings);
        updateValidityWithLatestFrame(target);
        target.living = target.living &&
                        isInsideTrackingBounds(target.predictedPosFrame, m_settings) &&
                        !latestFramesAreAllInvalid(target, m_settings.numFramesLiving,
                                                   RecentFrameWindowMode::UseAvailableFrames) &&
                        !latestValidMeasurementsRepeatEquatorialPoint(target);
    }
    return 0;
}

}  // namespace Dss::Tracking
