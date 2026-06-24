#include <algorithm>
#include <cmath>
#include <limits>

#include "dss/tracking/lifecycle_utils.h"
#include "geo_tracking_detail.h"

namespace Dss::Tracking::GeoDetail {

inline constexpr float kMaxGeoRediscoveryFrameSpeed = 100.0F;
inline constexpr int kGeoTrackingInvalidSearchWindow = 10;
inline constexpr int kGeoTrackingRadiusInvalidLimit = 5;
inline constexpr float kGeoTrackingRadiusInvalidStep = 10.0F;
inline constexpr float kGeoTrackingBaseSpeedErrorThreshold = 5.0F;
inline constexpr double kGeoRaDecTrackingRadiusArcsec = 50.0;

/// @brief 单次 GEO 跟踪匹配所需的坐标空间与门限集合。
struct GeoTrackingGateOptions {
    GeoTrackingSpace space = GeoTrackingSpace::Frame;  ///< 当前跟踪坐标空间
    CandidateMeasurementSpace measurementSpace =
        CandidateMeasurementSpace::Centroid;  ///< 用于距离比较的测量空间
    MeasurementReuseRule reuseRule{};         ///< 已使用像斑的判重规则
    float searchRadius = 0.0F;                ///< 像面搜索半径
    float frameSpeedErrorThreshold = 0.0F;    ///< 像面速度误差阈值
    float aePositionThreshold = 0.0F;         ///< 方位俯仰位置误差阈值
    float raDecTrackingRadius = 0.0F;         ///< 赤经赤纬跟踪半径
    float raSpeedThreshold = 0.0F;            ///< 赤经速度误差阈值
    float decSpeedThreshold = 0.0F;           ///< 赤纬速度误差阈值
};
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

// 搜索半径会随最近无效帧数扩大，并支持像面与赤经/赤纬两种跟踪空间。
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

}  // namespace Dss::Tracking::GeoDetail
