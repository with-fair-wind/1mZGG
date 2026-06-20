#include "dss/tracking/manual_tracker.h"

#include <cmath>
#include <optional>

#include "dss/core/constants.h"

namespace Dss::Tracking {

namespace {

inline constexpr float kManualBlobHalfExtent = 10.0F;
inline constexpr float kManualBlobArea = 100.0F;
inline constexpr float kManualBlobDn = 10000.0F;
inline constexpr float kArcsecPerDegree = 3600.0F;
inline constexpr double kMinCosElevation = 1.0e-6;

[[nodiscard]] auto boundedCosElevation(float elevationDegrees) -> double {
    const auto cosElevation = std::cos(static_cast<double>(elevationDegrees) * Dss::Core::DegToRad);
    if (std::abs(cosElevation) >= kMinCosElevation) {
        return cosElevation;
    }
    return cosElevation < 0.0 ? -kMinCosElevation : kMinCosElevation;
}

[[nodiscard]] auto framePeriodSeconds(const Dss::Core::FrameMeasurements& measurements) -> float {
    if (measurements.frameFreq > 0.0F) {
        return 1.0F / measurements.frameFreq;
    }
    return 1.0F;
}

/**
 * @brief 补全手动目标像斑的默认属性并计算方位-俯仰坐标
 *
 * 根据光学参数与视场中心，由像素质心推算 distAzi/distEle 及 posAe。
 */
[[nodiscard]] auto enrichManualBlob(Dss::Core::MeasuredBlob blob,
                                    const Dss::Core::FrameMeasurements& measurements,
                                    const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::MeasuredBlob {
    if (blob.id.empty()) {
        blob.id = "manual";
    }

    if (!(blob.maxX > blob.minX && blob.maxY > blob.minY)) {
        blob.minX = blob.centroid.x - kManualBlobHalfExtent;
        blob.maxX = blob.centroid.x + kManualBlobHalfExtent;
        blob.minY = blob.centroid.y - kManualBlobHalfExtent;
        blob.maxY = blob.centroid.y + kManualBlobHalfExtent;
    }
    if (blob.area <= 0.0F) {
        blob.area = kManualBlobArea;
    }
    if (blob.dn <= 0.0F) {
        blob.dn = kManualBlobDn;
    }

    const auto& optic = settings.opticParams;
    const auto distEle = (optic.fovCenterY - blob.centroid.y) * optic.pixelScale;
    const auto elevation = measurements.fovCenterAe.y + distEle;
    const auto distAzi = (blob.centroid.x - optic.fovCenterX) * optic.pixelScale /
                         static_cast<float>(boundedCosElevation(elevation));

    blob.posAe = Dss::Core::Vec2f{measurements.fovCenterAe.x + distAzi, elevation};
    blob.fovCenterAziModify = measurements.fovCenterAe.x;
    blob.fovCenterEleModify = measurements.fovCenterAe.y;
    blob.distAzi = distAzi;
    blob.distEle = distEle;
    blob.targetAzi = blob.posAe.x;
    blob.targetEle = blob.posAe.y;
    return blob;
}

[[nodiscard]] auto makeFrameInfo(const Dss::Core::FrameMeasurements& measurements,
                                 const Dss::Core::MeasuredBlob& blob,
                                 const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetFrameInfo {
    Dss::Core::TargetFrameInfo info{};
    info.timestamp = measurements.timestamp;
    info.frameSeq = measurements.frameSeq;
    info.fovCenterAe = measurements.fovCenterAe;
    info.opticCenter =
        Dss::Core::Vec2f{settings.opticParams.fovCenterX, settings.opticParams.fovCenterY};
    info.exposureTime = measurements.exposureTime;
    info.frameFreq = measurements.frameFreq;
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

}  // namespace

ManualTracker::ManualTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

/// 基于手动指定目标更新轨迹，并推算帧间速度与预测位置
auto ManualTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifo.push_back(measurements);
    if (m_fifo.size() > 10) {
        m_fifo.pop_front();
    }

    Dss::Core::MeasuredBlob blob{};
    {
        std::lock_guard lock(m_blobMutex);
        if (!m_hasManualBlob) {
            return {};
        }
        blob = m_manualBlob;
    }

    blob = enrichManualBlob(blob, measurements, m_settings);
    const auto info = makeFrameInfo(measurements, blob, m_settings);
    const auto previousInfo =
        m_currentTarget.frameInfos.empty()
            ? std::optional<Dss::Core::TargetFrameInfo>{}
            : std::optional<Dss::Core::TargetFrameInfo>{m_currentTarget.frameInfos.back()};

    if (!m_currentTarget.living) {
        m_currentTarget = {};
        m_currentTarget.targetId = blob.id;
        m_currentTarget.living = true;
        m_currentTarget.validity = 1.0F;
    }

    m_currentTarget.frameInfos.push_back(info);
    m_currentTarget.validity = 1.0F;
    m_currentTarget.living = true;
    m_currentTarget.lastRmDm =
        Dss::Core::Vec2f{blob.distAzi * kArcsecPerDegree, blob.distEle * kArcsecPerDegree};

    if (!previousInfo.has_value()) {
        m_currentTarget.predictedPosFrame = blob.centroid;
        m_currentTarget.predictedPosAe = blob.posAe;
        m_currentTarget.predictedSpdFrame = {};
        m_currentTarget.predictedSpdAe = {};
        return {m_currentTarget};
    }

    const auto period = framePeriodSeconds(measurements);
    const auto& previousBlob = previousInfo->measuredBlob;
    m_currentTarget.predictedSpdFrame = Dss::Core::Vec2f{blob.centroid.x - previousBlob.centroid.x,
                                                         blob.centroid.y - previousBlob.centroid.y};
    m_currentTarget.predictedPosFrame =
        Dss::Core::Vec2f{blob.centroid.x + m_currentTarget.predictedSpdFrame.x,
                         blob.centroid.y + m_currentTarget.predictedSpdFrame.y};
    m_currentTarget.predictedSpdAe =
        Dss::Core::Vec2f{(blob.posAe.x - previousBlob.posAe.x) / period,
                         (blob.posAe.y - previousBlob.posAe.y) / period};
    m_currentTarget.predictedPosAe =
        Dss::Core::Vec2f{blob.posAe.x + m_currentTarget.predictedSpdAe.x * period,
                         blob.posAe.y + m_currentTarget.predictedSpdAe.y * period};
    return {m_currentTarget};
}

void ManualTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
    std::lock_guard lock(m_blobMutex);
    m_manualBlob = {};
    m_hasManualBlob = false;
}

void ManualTracker::setManualTarget(const Dss::Core::MeasuredBlob& blob) {
    std::lock_guard lock(m_blobMutex);
    m_manualBlob = blob;
    m_hasManualBlob = true;
}

}  // namespace Dss::Tracking
