#include "dss/tracking/geo_tracker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "dss/core/constants.h"
#include "dss/tracking/candidate_utils.h"
#include "dss/tracking/lifecycle_utils.h"
#include "dss/tracking/prediction_utils.h"

namespace Dss::Tracking {

namespace {

inline constexpr int kReliableStarMatchCount = 5;
inline constexpr float kDefaultGeoAssociationRadius = 8.0F;
inline constexpr float kHighConfidenceGeoAssociationRadius = 30.0F;
inline constexpr float kDenseFrameAssociationRadius = 8.0F;
inline constexpr float kGeoMotionConsistencyThreshold = 2.5F;
inline constexpr float kStarLikeSpeedThreshold = 3.0F;
inline constexpr float kMaxGeoRediscoveryFrameSpeed = 100.0F;
inline constexpr std::size_t kMaxGeoRediscoveryBlobCount = 20000U;
inline constexpr int kGeoTrackingInvalidSearchWindow = 10;
inline constexpr int kGeoTrackingRadiusInvalidLimit = 5;
inline constexpr float kGeoTrackingRadiusInvalidStep = 10.0F;
inline constexpr float kGeoTrackingBaseSpeedErrorThreshold = 5.0F;
inline constexpr double kGeoRaDecTrackingRadiusArcsec = 50.0;
inline constexpr float kAngleAwayFromStarDegrees = 5.0F;
inline constexpr float kSoftDenominatorOffset = 0.001F;
inline constexpr double kGeoSameEquatorialThresholdDeg = 0.0015;
inline constexpr double kTinyCos = 1.0e-8;

/**
 * @brief GEO 关联与跟踪门限内部使用的坐标空间。
 */
enum class GeoTrackingSpace {
    Frame,  ///< 使用像面质心坐标。
    RaDec,  ///< 使用赤经/赤纬坐标。
};

/// GEO 跟踪阶段使用的动态门限参数。
struct GeoTrackingGateOptions {
    GeoTrackingSpace space = GeoTrackingSpace::Frame;  ///< 跟踪匹配使用的坐标空间。
    CandidateMeasurementSpace measurementSpace =
        CandidateMeasurementSpace::Centroid;  ///< 候选测量位置使用的坐标空间。
    MeasurementReuseRule reuseRule{};         ///< 当前帧测量复用判断规则。
    float searchRadius = 0.0F;                ///< 像面预测位置搜索半径（像素）。
    float frameSpeedErrorThreshold = 0.0F;    ///< 像面速度误差门限（像素/帧）。
    float aePositionThreshold = 0.0F;         ///< AE 预测位置门限。
    float raDecTrackingRadius = 0.0F;         ///< RA/Dec 预测位置搜索半径（弧度）。
    float raSpeedThreshold = 0.0F;            ///< RA 方向速度误差门限（弧度/帧）。
    float decSpeedThreshold = 0.0F;           ///< Dec 方向速度误差门限（弧度/帧）。
};

using Dss::Tracking::CandidateMeasurementSpace;
using Dss::Tracking::countRecentInvalidFrames;
using Dss::Tracking::deduplicateInitialCandidates;
using Dss::Tracking::InitialMeasurementDedupRule;
using Dss::Tracking::InvalidFallbackBlobOptions;
using Dss::Tracking::isMeasurementAlreadyUsed;
using Dss::Tracking::latestFramesAreAllInvalid;
using Dss::Tracking::makeInvalidFallbackBlob;
using Dss::Tracking::measurementMotion;
using Dss::Tracking::measurementPosition;
using Dss::Tracking::MeasurementReuseRule;
using Dss::Tracking::overlapsAnyLivingTarget;
using Dss::Tracking::RecentFrameWindowMode;
using Dss::Tracking::RecentMeasurementOverlapRule;
using Dss::Tracking::updateValidityWithLatestFrame;

[[nodiscard]] auto geoTrackingSpace(const Dss::Core::TrackingSettings& settings)
    -> GeoTrackingSpace {
    return settings.geoFullLeo ? GeoTrackingSpace::Frame : GeoTrackingSpace::RaDec;
}

/**
 * @brief 将 GEO 跟踪坐标空间映射为候选去重使用的测量空间。
 */
[[nodiscard]] auto candidateMeasurementSpace(GeoTrackingSpace space) -> CandidateMeasurementSpace {
    return space == GeoTrackingSpace::RaDec ? CandidateMeasurementSpace::RaDec
                                            : CandidateMeasurementSpace::Centroid;
}

[[nodiscard]] auto arcsecToRad(double arcsec) -> float {
    return static_cast<float>(arcsec * Dss::Core::ArcSecToRad);
}

[[nodiscard]] auto timestampSeconds(const Dss::Core::Timestamp& timestamp) -> double {
    return static_cast<double>(timestamp.hour * 3600 + timestamp.minute * 60 + timestamp.second) +
           static_cast<double>(timestamp.millisecond) * 0.001 +
           static_cast<double>(timestamp.microsecond) * 0.000001;
}

[[nodiscard]] auto framePeriodSeconds(const Dss::Core::FrameMeasurements& previous,
                                      const Dss::Core::FrameMeasurements& current) -> double {
    const auto previousSeconds = timestampSeconds(previous.timestamp);
    auto currentSeconds = timestampSeconds(current.timestamp);
    if (previous.timestamp.day != current.timestamp.day) {
        currentSeconds += 86400.0;
    }

    const auto diff = currentSeconds - previousSeconds;
    if (diff > 0.0) {
        return diff;
    }
    if (current.frameFreq > 0.0F) {
        return 1.0 / static_cast<double>(current.frameFreq);
    }
    if (previous.frameFreq > 0.0F) {
        return 1.0 / static_cast<double>(previous.frameFreq);
    }
    return 1.0;
}

[[nodiscard]] bool isInsideCenterWindow(const Dss::Core::MeasuredBlob& blob, float ratioFov,
                                        const Dss::Core::OpticParams& opticParams) {
    const auto minX = (1.0F - ratioFov) * static_cast<float>(opticParams.imageWidth) / 2.0F;
    const auto maxX = (1.0F + ratioFov) * static_cast<float>(opticParams.imageWidth) / 2.0F;
    const auto minY = (1.0F - ratioFov) * static_cast<float>(opticParams.imageHeight) / 2.0F;
    const auto maxY = (1.0F + ratioFov) * static_cast<float>(opticParams.imageHeight) / 2.0F;
    return blob.centroid.x > minX && blob.centroid.x < maxX && blob.centroid.y > minY &&
           blob.centroid.y < maxY;
}

[[nodiscard]] auto truncateTowardZero(float value) -> int {
    return static_cast<int>(std::trunc(value));
}

[[nodiscard]] auto findMostFrequentMotion(std::span<const std::pair<int, int>> motions)
    -> std::optional<std::pair<std::pair<int, int>, int>> {
    if (motions.empty()) {
        return std::nullopt;
    }

    std::map<std::pair<int, int>, int> counts;
    for (const auto& motion : motions) {
        ++counts[motion];
    }

    std::pair<int, int> bestMotion{};
    int bestCount = 0;
    for (const auto& [motion, count] : counts) {
        if (count > bestCount) {
            bestMotion = motion;
            bestCount = count;
        }
    }
    return std::pair{bestMotion, bestCount};
}

/**
 * @brief 由相邻两帧恒星测量估算背景恒星速度。
 *
 *
 * 在视场中心区域内匹配恒星对，统计出现频率最高的像素位移作为背景像面速度，

 * 并结合视场中心方位/俯仰变化换算 AE 速度。
 */
[[nodiscard]] auto estimateGeoStarSpeedFromPair(const Dss::Core::FrameMeasurements& previous,
                                                const Dss::Core::FrameMeasurements& current,
                                                float ratioFov, float radius,
                                                const Dss::Core::OpticParams& opticParams)
    -> GeoStarSpeedResult {
    if (previous.starBlobs.empty() || current.starBlobs.empty()) {
        return GeoStarSpeedResult{GeoStarSpeedStatus::MissingStars};
    }

    std::vector<std::pair<int, int>> candidateMotions;
    for (const auto& previousStar : previous.starBlobs) {
        if (!isInsideCenterWindow(previousStar, ratioFov, opticParams)) {
            continue;
        }
        for (const auto& currentStar : current.starBlobs) {
            if (!isInsideCenterWindow(currentStar, ratioFov, opticParams)) {
                continue;
            }

            const auto dx = truncateTowardZero(currentStar.centroid.x - previousStar.centroid.x);
            const auto dy = truncateTowardZero(currentStar.centroid.y - previousStar.centroid.y);
            if (std::abs(dx) < radius && std::abs(dy) < radius) {
                candidateMotions.emplace_back(dx, dy);
            }
        }
    }

    const auto mostFrequent = findMostFrequentMotion(candidateMotions);
    if (!mostFrequent.has_value()) {
        return GeoStarSpeedResult{GeoStarSpeedStatus::NoCandidates};
    }

    const auto [motion, matchCount] = *mostFrequent;
    const auto period = framePeriodSeconds(previous, current);
    const auto frameFrequency = 1.0 / period;
    const auto frameSpeed =
        Dss::Core::Vec2f{static_cast<float>(motion.first), static_cast<float>(motion.second)};
    const auto aeSpeed = Dss::Core::Vec2f{
        static_cast<float>(((current.fovCenterAe.x - previous.fovCenterAe.x) +
                            frameSpeed.x * opticParams.pixelScale) *
                           frameFrequency),
        static_cast<float>(((current.fovCenterAe.y - previous.fovCenterAe.y) +
                            frameSpeed.y * opticParams.pixelScale) *
                           frameFrequency),
    };

    return GeoStarSpeedResult{GeoStarSpeedStatus::Ok, frameSpeed, aeSpeed, matchCount};
}

[[nodiscard]] auto median3(float a, float b, float c) -> float {
    const auto maxValue = std::max({a, b, c});
    const auto minValue = std::min({a, b, c});
    return a + b + c - maxValue - minValue;
}

[[nodiscard]] auto softDenominator(float value) -> float {
    return value < 0.0F ? value - kSoftDenominatorOffset : value + kSoftDenominatorOffset;
}

[[nodiscard]] bool isMotionStarLike(const Dss::Core::Vec2f& motion,
                                    const Dss::Core::Vec2f& starSpeed) {
    return std::abs(motion.x - starSpeed.x) < kStarLikeSpeedThreshold &&
           std::abs(motion.y - starSpeed.y) < kStarLikeSpeedThreshold;
}

[[nodiscard]] bool isMotionAngleAwayFromStars(const Dss::Core::Vec2f& motion,
                                              const Dss::Core::Vec2f& starSpeed) {
    const auto motionAngle = std::atan(motion.x / softDenominator(motion.y));
    const auto starAngle = std::atan(starSpeed.x / softDenominator(starSpeed.y));
    return std::abs(motionAngle - starAngle) >
           kAngleAwayFromStarDegrees * static_cast<float>(Dss::Core::DegToRad);
}

[[nodiscard]] bool hasEquatorialCoordinates(const Dss::Core::MeasuredBlob& blob) {
    return blob.alpha != 0.0 || blob.sigma != 0.0 || blob.ra != 0.0 || blob.dec != 0.0;
}

[[nodiscard]] bool isSameEquatorialPoint(const Dss::Core::MeasuredBlob& first,
                                         const Dss::Core::MeasuredBlob& second) {
    if (!hasEquatorialCoordinates(first) || !hasEquatorialCoordinates(second)) {
        return false;
    }

    const auto threshold =
        kGeoSameEquatorialThresholdDeg / (std::cos(first.sigma) + kTinyCos) * Dss::Core::DegToRad;
    return std::abs(first.alpha - second.alpha) < threshold &&
           std::abs(first.sigma - second.sigma) < threshold;
}

[[nodiscard]] bool hasRepeatedEquatorialPoint(
    const std::array<const Dss::Core::MeasuredBlob*, 4>& blobs) {
    for (std::size_t i = 0; i < blobs.size(); ++i) {
        for (std::size_t j = i + 1; j < blobs.size(); ++j) {
            if (isSameEquatorialPoint(*blobs[i], *blobs[j])) {
                return true;
            }
        }
    }
    return false;
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

/**
 * @brief 根据目标最近历史帧的三段运动中位数更新预测位置与速度。
 */
void updatePredictionFromHistory(Dss::Core::TargetInfo& target, float frameFrequency,
                                 const Dss::Core::TrackingSettings& settings) {
    if (target.frameInfos.empty()) {
        return;
    }

    const auto period = frameFrequency > 0.0F ? 1.0F / frameFrequency : 1.0F;
    const auto& latest = target.frameInfos.back().measuredBlob;
    const auto space = geoTrackingSpace(settings);
    const auto measurementSpace = candidateMeasurementSpace(space);
    const auto positionOf = [measurementSpace](const Dss::Core::MeasuredBlob& blob) {
        return measurementPosition(blob, measurementSpace).value_or(Dss::Core::Vec2f{});
    };

    if (target.frameInfos.size() < 2) {
        target.predictedSpdFrame = {};
        target.predictedSpdAe = {};
        target.predictedPosFrame = positionOf(latest);
        target.predictedPosAe = latest.posAe;
        return;
    }

    const auto lastIndex = target.frameInfos.size() - 1U;
    const auto motionAt = [&](std::size_t index) {
        const auto& current = target.frameInfos[index].measuredBlob;
        const auto& previous = target.frameInfos[index - 1U].measuredBlob;
        const auto currentPosition = positionOf(current);
        const auto previousPosition = positionOf(previous);
        return Dss::Core::Vec2f{currentPosition.x - previousPosition.x,
                                currentPosition.y - previousPosition.y};
    };
    const auto aeMotionAt = [&](std::size_t index) {
        const auto& current = target.frameInfos[index].measuredBlob;
        const auto& previous = target.frameInfos[index - 1U].measuredBlob;
        return Dss::Core::Vec2f{current.posAe.x - previous.posAe.x,
                                current.posAe.y - previous.posAe.y};
    };

    auto frameMotion = motionAt(lastIndex);
    auto aeMotion = aeMotionAt(lastIndex);
    if (target.frameInfos.size() >= 4) {
        const auto motion1 = motionAt(lastIndex);
        const auto motion2 = motionAt(lastIndex - 1U);
        const auto motion3 = motionAt(lastIndex - 2U);
        frameMotion = Dss::Core::Vec2f{median3(motion1.x, motion2.x, motion3.x),
                                       median3(motion1.y, motion2.y, motion3.y)};

        const auto aeMotion1 = aeMotionAt(lastIndex);
        const auto aeMotion2 = aeMotionAt(lastIndex - 1U);
        const auto aeMotion3 = aeMotionAt(lastIndex - 2U);
        aeMotion = Dss::Core::Vec2f{median3(aeMotion1.x, aeMotion2.x, aeMotion3.x),
                                    median3(aeMotion1.y, aeMotion2.y, aeMotion3.y)};
    }

    target.predictedSpdFrame = frameMotion;
    const auto latestPosition = positionOf(latest);
    target.predictedPosFrame =
        Dss::Core::Vec2f{latestPosition.x + frameMotion.x, latestPosition.y + frameMotion.y};
    target.predictedSpdAe = Dss::Core::Vec2f{aeMotion.x / period, aeMotion.y / period};
    target.predictedPosAe = Dss::Core::Vec2f{latest.posAe.x + target.predictedSpdAe.x * period,
                                             latest.posAe.y + target.predictedSpdAe.y * period};
    target.lastRmDm = Dss::Core::Vec2f{latest.distAzi, latest.distEle};
}

[[nodiscard]] auto makeAssociatedTarget(
    const std::array<const Dss::Core::FrameMeasurements*, 4>& frames,
    const std::array<const Dss::Core::MeasuredBlob*, 4>& blobs, float frameFrequency,
    const Dss::Core::TrackingSettings& settings) -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.validity = 1.0F;
    target.living = true;

    for (std::size_t index = 0; index < frames.size(); ++index) {
        target.frameInfos.push_back(makeFrameInfo(*frames[index], *blobs[index], settings));
    }
    updatePredictionFromHistory(target, frameFrequency, settings);
    return target;
}

[[nodiscard]] auto associationRadius(const Dss::Core::TrackingSettings& settings,
                                     int starMatchCount, std::size_t latestTargetCount) -> float {
    auto radius =
        starMatchCount >= 15 ? kHighConfidenceGeoAssociationRadius : kDefaultGeoAssociationRadius;
    if (latestTargetCount > 3000U) {
        radius = kDenseFrameAssociationRadius;
    }
    return std::min(radius, settings.searchRadius);
}

[[nodiscard]] bool matchesFrameAssociationMotion(const Dss::Core::Vec2f& motion,
                                                 const Dss::Core::Vec2f& starSpeed, float radius) {
    return std::abs(motion.x) < radius && std::abs(motion.y) < radius &&
           !isMotionStarLike(motion, starSpeed) && isMotionAngleAwayFromStars(motion, starSpeed);
}

[[nodiscard]] bool matchesConsistentFrameAssociationMotion(const Dss::Core::Vec2f& motion,
                                                           const Dss::Core::Vec2f& previousMotion,
                                                           const Dss::Core::Vec2f& firstMotion,
                                                           const Dss::Core::Vec2f& starSpeed,
                                                           float radius) {
    return matchesFrameAssociationMotion(motion, starSpeed, radius) &&
           std::abs(motion.x - previousMotion.x) <= kGeoMotionConsistencyThreshold &&
           std::abs(motion.y - previousMotion.y) <= kGeoMotionConsistencyThreshold &&
           std::abs(motion.x - firstMotion.x) <= kGeoMotionConsistencyThreshold &&
           std::abs(motion.y - firstMotion.y) <= kGeoMotionConsistencyThreshold;
}

[[nodiscard]] bool isWithinRaDecAssociationRadius(const Dss::Core::Vec2f& motion) {
    const auto radius = arcsecToRad(kGeoRaDecTrackingRadiusArcsec);
    return std::abs(motion.x) < radius && std::abs(motion.y) < radius;
}

[[nodiscard]] bool exceedsRaDecMinimumMotion(const Dss::Core::Vec2f& motion,
                                             const Dss::Core::TrackingSettings& settings) {
    const auto raThreshold = arcsecToRad(settings.geoRaThresholdArcsec);
    const auto decThreshold = arcsecToRad(settings.geoDecThresholdArcsec);
    return !(std::abs(motion.x) < raThreshold && std::abs(motion.y) < decThreshold);
}

[[nodiscard]] bool matchesRaDecSpeedConsistency(const Dss::Core::Vec2f& motion,
                                                const Dss::Core::Vec2f& referenceMotion,
                                                const Dss::Core::TrackingSettings& settings) {
    const auto raSpeedThreshold = arcsecToRad(settings.geoRaSpeedThresholdArcsec);
    const auto decSpeedThreshold = arcsecToRad(settings.geoDecSpeedThresholdArcsec);
    return std::abs(motion.x - referenceMotion.x) <= raSpeedThreshold &&
           std::abs(motion.y - referenceMotion.y) <= decSpeedThreshold;
}

[[nodiscard]] bool matchesRaDecAssociationMotion(const Dss::Core::Vec2f& motion,
                                                 const Dss::Core::TrackingSettings& settings) {
    return isWithinRaDecAssociationRadius(motion) && exceedsRaDecMinimumMotion(motion, settings);
}

[[nodiscard]] bool matchesConsistentRaDecAssociationMotion(
    const Dss::Core::Vec2f& motion, const Dss::Core::Vec2f& previousMotion,
    const Dss::Core::Vec2f& firstMotion, const Dss::Core::TrackingSettings& settings) {
    return matchesRaDecAssociationMotion(motion, settings) &&
           matchesRaDecSpeedConsistency(motion, previousMotion, settings) &&
           matchesRaDecSpeedConsistency(motion, firstMotion, settings);
}

[[nodiscard]] bool matchesFirstAssociationMotion(const Dss::Core::Vec2f& motion,
                                                 GeoTrackingSpace space,
                                                 const Dss::Core::Vec2f& starSpeed,
                                                 float frameRadius,
                                                 const Dss::Core::TrackingSettings& settings) {
    return space == GeoTrackingSpace::RaDec
               ? matchesRaDecAssociationMotion(motion, settings)
               : matchesFrameAssociationMotion(motion, starSpeed, frameRadius);
}

[[nodiscard]] bool matchesSecondAssociationMotion(const Dss::Core::Vec2f& motion,
                                                  const Dss::Core::Vec2f& firstMotion,
                                                  GeoTrackingSpace space,
                                                  const Dss::Core::Vec2f& starSpeed,
                                                  float frameRadius,
                                                  const Dss::Core::TrackingSettings& settings) {
    return space == GeoTrackingSpace::RaDec
               ? matchesConsistentRaDecAssociationMotion(motion, firstMotion, firstMotion, settings)
               : matchesConsistentFrameAssociationMotion(motion, firstMotion, firstMotion,
                                                         starSpeed, frameRadius);
}

[[nodiscard]] bool matchesThirdAssociationMotion(
    const Dss::Core::Vec2f& motion, const Dss::Core::Vec2f& previousMotion,
    const Dss::Core::Vec2f& firstMotion, GeoTrackingSpace space, const Dss::Core::Vec2f& starSpeed,
    float frameRadius, const Dss::Core::TrackingSettings& settings) {
    return space == GeoTrackingSpace::RaDec
               ? matchesConsistentRaDecAssociationMotion(motion, previousMotion, firstMotion,
                                                         settings)
               : matchesConsistentFrameAssociationMotion(motion, previousMotion, firstMotion,
                                                         starSpeed, frameRadius);
}

/**
 * @brief 对最近四帧目标像斑执行链式关联，生成 GEO 初始候选。
 *
 *
 * 过滤与背景恒星运动一致的候选，并在当前测量空间中去除重复关联链。

 */
[[nodiscard]] auto associateFourFrameTargets(
    const std::deque<Dss::Core::FrameMeasurements>& fifoTarget, int starMatchCount,
    const Dss::Core::Vec2f& starSpeed, float frameFrequency,
    const Dss::Core::TrackingSettings& settings) -> std::vector<Dss::Core::TargetInfo> {
    if (fifoTarget.size() < 4U) {
        return {};
    }

    const auto frameCount = fifoTarget.size();
    const auto& frame0 = fifoTarget[frameCount - 4U];
    const auto& frame1 = fifoTarget[frameCount - 3U];
    const auto& frame2 = fifoTarget[frameCount - 2U];
    const auto& frame3 = fifoTarget[frameCount - 1U];
    if (frame0.targetBlobs.empty() || frame1.targetBlobs.empty() || frame2.targetBlobs.empty() ||
        frame3.targetBlobs.empty()) {
        return {};
    }

    const auto space = geoTrackingSpace(settings);
    const auto measurementSpace = candidateMeasurementSpace(space);
    const auto frameRadius = associationRadius(settings, starMatchCount, frame3.targetBlobs.size());
    std::vector<Dss::Core::TargetInfo> associatedTargets;

    for (const auto& blob0 : frame0.targetBlobs) {
        for (const auto& blob1 : frame1.targetBlobs) {
            const auto motion10 = measurementMotion(blob1, blob0, measurementSpace);
            if (!motion10.has_value() || !matchesFirstAssociationMotion(*motion10, space, starSpeed,
                                                                        frameRadius, settings)) {
                continue;
            }

            for (const auto& blob2 : frame2.targetBlobs) {
                const auto motion21 = measurementMotion(blob2, blob1, measurementSpace);
                if (!motion21.has_value() ||
                    !matchesSecondAssociationMotion(*motion21, *motion10, space, starSpeed,
                                                    frameRadius, settings)) {
                    continue;
                }

                for (const auto& blob3 : frame3.targetBlobs) {
                    const auto motion32 = measurementMotion(blob3, blob2, measurementSpace);
                    if (!motion32.has_value() ||
                        !matchesThirdAssociationMotion(*motion32, *motion21, *motion10, space,
                                                       starSpeed, frameRadius, settings)) {
                        continue;
                    }

                    const std::array blobs{&blob0, &blob1, &blob2, &blob3};
                    if (hasRepeatedEquatorialPoint(blobs)) {
                        continue;
                    }
                    const std::array frames{&frame0, &frame1, &frame2, &frame3};
                    associatedTargets.push_back(
                        makeAssociatedTarget(frames, blobs, frameFrequency, settings));
                }
            }
        }
    }

    InitialMeasurementDedupRule dedupRule{};
    dedupRule.frameCount = 3U;
    return deduplicateInitialCandidates(std::move(associatedTargets), dedupRule, measurementSpace);
}

[[nodiscard]] bool exceedsRediscoverySpeedLimit(const Dss::Core::TargetInfo& target) {
    return std::abs(target.predictedSpdFrame.x) > kMaxGeoRediscoveryFrameSpeed ||
           std::abs(target.predictedSpdFrame.y) > kMaxGeoRediscoveryFrameSpeed;
}

[[nodiscard]] auto trackingSearchRadius(float baseRadius, int recentInvalidCount) -> float {
    return baseRadius +
           static_cast<float>(std::min(recentInvalidCount, kGeoTrackingRadiusInvalidLimit)) *
               kGeoTrackingRadiusInvalidStep;
}

[[nodiscard]] auto trackingSpeedErrorThreshold(int recentInvalidCount) -> float {
    return kGeoTrackingBaseSpeedErrorThreshold + static_cast<float>(recentInvalidCount);
}

[[nodiscard]] auto makeTrackingGateOptions(const Dss::Core::TrackingSettings& settings,
                                           int recentInvalidCount) -> GeoTrackingGateOptions {
    const auto space = geoTrackingSpace(settings);
    const auto measurementSpace = candidateMeasurementSpace(space);
    MeasurementReuseRule reuseRule{};
    reuseRule.space = measurementSpace;
    reuseRule.matchAnyRaDecAxis = space == GeoTrackingSpace::RaDec;
    return GeoTrackingGateOptions{
        .space = space,
        .measurementSpace = measurementSpace,
        .reuseRule = reuseRule,
        .searchRadius = trackingSearchRadius(settings.searchRadius, recentInvalidCount),
        .frameSpeedErrorThreshold = trackingSpeedErrorThreshold(recentInvalidCount),
        .aePositionThreshold = settings.thresholdAe,
        .raDecTrackingRadius = arcsecToRad(kGeoRaDecTrackingRadiusArcsec),
        .raSpeedThreshold = arcsecToRad(settings.geoRaSpeedThresholdArcsec),
        .decSpeedThreshold = arcsecToRad(settings.geoDecSpeedThresholdArcsec),
    };
}

[[nodiscard]] bool matchesAePositionGate(const Dss::Core::MeasuredBlob& candidate,
                                         const Dss::Core::TargetInfo& target,
                                         float aePositionThreshold) {
    if (aePositionThreshold <= 0.0F) {
        return true;
    }

    return std::abs(candidate.posAe.x - target.predictedPosAe.x) <= aePositionThreshold &&
           std::abs(candidate.posAe.y - target.predictedPosAe.y) <= aePositionThreshold;
}

[[nodiscard]] bool matchesFrameTrackingGate(const Dss::Core::MeasuredBlob& candidate,
                                            const Dss::Core::TargetInfo& target,
                                            const GeoTrackingGateOptions& options) {
    if (target.frameInfos.empty()) {
        return false;
    }

    const auto distanceFromPrediction =
        Dss::Core::Vec2f{target.predictedPosFrame.x - candidate.centroid.x,
                         target.predictedPosFrame.y - candidate.centroid.y};
    const auto& latestBlob = target.frameInfos.back().measuredBlob;
    const auto measuredFrameMotion = Dss::Core::Vec2f{candidate.centroid.x - latestBlob.centroid.x,
                                                      candidate.centroid.y - latestBlob.centroid.y};
    return std::abs(distanceFromPrediction.x) <= options.searchRadius &&
           std::abs(distanceFromPrediction.y) <= options.searchRadius &&
           std::abs(measuredFrameMotion.x - target.predictedSpdFrame.x) <
               options.frameSpeedErrorThreshold &&
           std::abs(measuredFrameMotion.y - target.predictedSpdFrame.y) <
               options.frameSpeedErrorThreshold &&
           matchesAePositionGate(candidate, target, options.aePositionThreshold);
}

[[nodiscard]] bool matchesRaDecTrackingGate(const Dss::Core::MeasuredBlob& candidate,
                                            const Dss::Core::TargetInfo& target,
                                            const GeoTrackingGateOptions& options) {
    if (target.frameInfos.empty()) {
        return false;
    }

    const auto& latestBlob = target.frameInfos.back().measuredBlob;
    const auto candidatePosition = measurementPosition(candidate, CandidateMeasurementSpace::RaDec);
    const auto latestPosition = measurementPosition(latestBlob, CandidateMeasurementSpace::RaDec);
    if (!candidatePosition.has_value() || !latestPosition.has_value()) {
        return false;
    }

    const auto measuredMotion = Dss::Core::Vec2f{candidatePosition->x - latestPosition->x,
                                                 candidatePosition->y - latestPosition->y};
    return std::abs(target.predictedPosFrame.x - candidatePosition->x) <=
               options.raDecTrackingRadius &&
           std::abs(target.predictedPosFrame.y - candidatePosition->y) <=
               options.raDecTrackingRadius &&
           std::abs(measuredMotion.x - target.predictedSpdFrame.x) < options.raSpeedThreshold &&
           std::abs(measuredMotion.y - target.predictedSpdFrame.y) < options.decSpeedThreshold;
}

/**
 * @brief 在当前帧中查找满足 GEO 跟踪门限的最近像斑。
 *
 *
 * 搜索半径会随最近无效帧数扩大，并支持像面与赤经/赤纬两种跟踪空间。

 */
[[nodiscard]] auto findNearestTrackedBlob(const std::vector<Dss::Core::MeasuredBlob>& blobs,
                                          const Dss::Core::TargetInfo& target,
                                          const Dss::Core::TrackingSettings& settings,
                                          const std::vector<Dss::Core::MeasuredBlob>& usedBlobs)
    -> std::optional<Dss::Core::MeasuredBlob> {
    const auto recentInvalidCount =
        countRecentInvalidFrames(target, kGeoTrackingInvalidSearchWindow);
    const auto options = makeTrackingGateOptions(settings, recentInvalidCount);
    auto bestDistance = std::numeric_limits<float>::max();
    std::optional<Dss::Core::MeasuredBlob> bestBlob;
    for (const auto& blob : blobs) {
        if (isMeasurementAlreadyUsed(blob, usedBlobs, options.reuseRule)) {
            continue;
        }
        const auto matched = options.space == GeoTrackingSpace::RaDec
                                 ? matchesRaDecTrackingGate(blob, target, options)
                                 : matchesFrameTrackingGate(blob, target, options);
        if (!matched) {
            continue;
        }

        const auto candidatePosition = measurementPosition(blob, options.measurementSpace);
        if (!candidatePosition.has_value()) {
            continue;
        }
        const auto dx = candidatePosition->x - target.predictedPosFrame.x;
        const auto dy = candidatePosition->y - target.predictedPosFrame.y;
        const auto distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestBlob = blob;
        }
    }
    return bestBlob;
}

[[nodiscard]] bool isInsideImageBounds(const Dss::Core::Vec2f& position,
                                       const Dss::Core::OpticParams& opticParams) {
    return position.x > 0.0F && position.x < static_cast<float>(opticParams.imageWidth) &&
           position.y > 0.0F && position.y < static_cast<float>(opticParams.imageHeight);
}

[[nodiscard]] bool isInsideRaDecBounds(const Dss::Core::Vec2f& position) {
    return position.x > 0.0F && position.x < static_cast<float>(2.0 * Dss::Core::Pi) &&
           position.y > 0.0F && position.y < static_cast<float>(Dss::Core::Pi / 2.0);
}

[[nodiscard]] bool isInsideTrackingBounds(const Dss::Core::Vec2f& position,
                                          const Dss::Core::TrackingSettings& settings) {
    return geoTrackingSpace(settings) == GeoTrackingSpace::RaDec
               ? isInsideRaDecBounds(position)
               : isInsideImageBounds(position, settings.opticParams);
}

[[nodiscard]] bool latestValidMeasurementsRepeatEquatorialPoint(
    const Dss::Core::TargetInfo& target) {
    if (target.frameInfos.size() < 2U) {
        return false;
    }

    const auto& latest = target.frameInfos.back();
    const auto& previous = target.frameInfos[target.frameInfos.size() - 2U];
    if (!latest.valid || !previous.valid) {
        return false;
    }
    return isSameEquatorialPoint(previous.measuredBlob, latest.measuredBlob);
}

[[nodiscard]] bool hasLivingTarget(const std::vector<Dss::Core::TargetInfo>& targets) {
    return std::ranges::any_of(targets,
                               [](const Dss::Core::TargetInfo& target) { return target.living; });
}

[[nodiscard]] auto makeGeoTargetId(uint64_t targetNumber) -> std::string {
    return "geo-" + std::to_string(targetNumber);
}

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
    return estimateGeoStarSpeedFromPair(frames[frames.size() - 2U], frames.back(), ratioFov, radius,
                                        opticParams);
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
