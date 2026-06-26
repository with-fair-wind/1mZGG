#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <utility>

#include "dss/core/constants.h"
#include "geo_tracking_detail.h"

namespace Dss::Tracking::GeoDetail {

inline constexpr float kDefaultGeoAssociationRadius = 8.0F;
inline constexpr float kHighConfidenceGeoAssociationRadius = 30.0F;
inline constexpr float kDenseFrameAssociationRadius = 8.0F;
inline constexpr float kGeoMotionConsistencyThreshold = 2.5F;
inline constexpr float kStarLikeSpeedThreshold = 3.0F;
inline constexpr double kGeoRaDecTrackingRadiusArcsec = 50.0;
inline constexpr float kAngleAwayFromStarDegrees = 5.0F;
inline constexpr float kSoftDenominatorOffset = 0.001F;
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

}  // namespace Dss::Tracking::GeoDetail
