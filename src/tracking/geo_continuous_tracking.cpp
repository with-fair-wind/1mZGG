#include <algorithm>
#include <cmath>
#include <string>

#include "dss/core/constants.h"
#include "geo_tracking_detail.h"

namespace Dss::Tracking::GeoDetail {

inline constexpr double kGeoSameEquatorialThresholdDeg = 0.0015;
inline constexpr double kTinyCos = 1.0e-8;
[[nodiscard]] bool isSameEquatorialPoint(const Dss::Core::MeasuredBlob& first,
                                         const Dss::Core::MeasuredBlob& second) {
    // 仅当双方都有 alpha/sigma 时比较;仅有 ra/dec 的由 RaDec 跟踪链路单独处理,
    // 不在此处用 alpha/sigma(为 0)误判为「相同点」
    const auto hasAlphaSigma = [](const Dss::Core::MeasuredBlob& blob) {
        return blob.alpha != 0.0 || blob.sigma != 0.0;
    };
    if (!hasAlphaSigma(first) || !hasAlphaSigma(second)) {
        return false;
    }
    // 赤经阈值按 cos(dec) 放大(赤经线在高赤纬汇聚);赤纬沿经线,不缩放
    const auto raThreshold =
        kGeoSameEquatorialThresholdDeg / (std::cos(first.sigma) + kTinyCos) * Dss::Core::DegToRad;
    const auto decThreshold = kGeoSameEquatorialThresholdDeg * Dss::Core::DegToRad;
    return std::abs(first.alpha - second.alpha) < raThreshold &&
           std::abs(first.sigma - second.sigma) < decThreshold;
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
    info.posZxdw = (blob.targetAzi != 0.0F || blob.targetEle != 0.0F)
                       ? Dss::Core::Vec2f{blob.targetAzi, blob.targetEle}
                       : blob.posAe;
    info.posTwdw = (blob.ra != 0.0 || blob.dec != 0.0)
                       ? Dss::Core::Vec2f{static_cast<float>(blob.ra), static_cast<float>(blob.dec)}
                       : blob.posAe;
    info.magnitude = blob.magnitude;
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

[[nodiscard]] bool isInsideImageBounds(const Dss::Core::Vec2f& position,
                                       const Dss::Core::OpticParams& opticParams) {
    return position.x > 0.0F && position.x < static_cast<float>(opticParams.imageWidth) &&
           position.y > 0.0F && position.y < static_cast<float>(opticParams.imageHeight);
}

[[nodiscard]] bool isInsideRaDecBounds(const Dss::Core::Vec2f& position) {
    // 赤经 [0, 2π)(允许 RA=0 春分点);赤纬 [-π/2, π/2](允许南天与赤道,不再拒绝 dec<0)
    return position.x >= 0.0F && position.x < static_cast<float>(2.0 * Dss::Core::Pi) &&
           position.y >= static_cast<float>(-Dss::Core::Pi / 2.0) &&
           position.y <= static_cast<float>(Dss::Core::Pi / 2.0);
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

}  // namespace Dss::Tracking::GeoDetail
