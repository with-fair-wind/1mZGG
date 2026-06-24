#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "dss/tracking/candidate_utils.h"
#include "dss/tracking/geo_tracker.h"

namespace Dss::Tracking::GeoDetail {

enum class GeoTrackingSpace {
    Frame,
    RaDec,
};

[[nodiscard]] auto geoTrackingSpace(const Dss::Core::TrackingSettings& settings)
    -> GeoTrackingSpace;
[[nodiscard]] auto candidateMeasurementSpace(GeoTrackingSpace space) -> CandidateMeasurementSpace;
[[nodiscard]] auto arcsecToRad(double arcsec) -> float;
[[nodiscard]] auto median3(float a, float b, float c) -> float;
[[nodiscard]] auto framePeriodSeconds(const Dss::Core::FrameMeasurements& previous,
                                      const Dss::Core::FrameMeasurements& current) -> double;
[[nodiscard]] auto estimateGeoStarSpeedFromPair(const Dss::Core::FrameMeasurements& previous,
                                                const Dss::Core::FrameMeasurements& current,
                                                float ratioFov, float radius,
                                                const Dss::Core::OpticParams& opticParams)
    -> GeoStarSpeedResult;

[[nodiscard]] bool hasRepeatedEquatorialPoint(
    const std::array<const Dss::Core::MeasuredBlob*, 4>& blobs);
[[nodiscard]] auto makeFrameInfo(const Dss::Core::FrameMeasurements& frame,
                                 const Dss::Core::MeasuredBlob& blob,
                                 const Dss::Core::TrackingSettings& settings)
    -> Dss::Core::TargetFrameInfo;
void updatePredictionFromHistory(Dss::Core::TargetInfo& target, float frameFrequency,
                                 const Dss::Core::TrackingSettings& settings);
[[nodiscard]] auto associateFourFrameTargets(
    const std::deque<Dss::Core::FrameMeasurements>& fifoTarget, int starMatchCount,
    const Dss::Core::Vec2f& starSpeed, float frameFrequency,
    const Dss::Core::TrackingSettings& settings) -> std::vector<Dss::Core::TargetInfo>;

[[nodiscard]] bool exceedsRediscoverySpeedLimit(const Dss::Core::TargetInfo& target);

/**
 * @brief 在当前帧中查找满足 GEO 跟踪门限的最近像斑。
 * @param blobs 当前帧候选像斑。
 * @param target 带有预测位置和速度的目标。
 * @param settings GEO 跟踪门限设置。
 * @param usedBlobs 已被其他目标占用的像斑。
 * @return 最近的可用像斑；没有候选通过门限时返回 std::nullopt。
 */
[[nodiscard]] auto findNearestTrackedBlob(const std::vector<Dss::Core::MeasuredBlob>& blobs,
                                          const Dss::Core::TargetInfo& target,
                                          const Dss::Core::TrackingSettings& settings,
                                          const std::vector<Dss::Core::MeasuredBlob>& usedBlobs)
    -> std::optional<Dss::Core::MeasuredBlob>;

[[nodiscard]] bool isInsideTrackingBounds(const Dss::Core::Vec2f& position,
                                          const Dss::Core::TrackingSettings& settings);
[[nodiscard]] bool latestValidMeasurementsRepeatEquatorialPoint(
    const Dss::Core::TargetInfo& target);
[[nodiscard]] bool hasLivingTarget(const std::vector<Dss::Core::TargetInfo>& targets);
[[nodiscard]] auto makeGeoTargetId(uint64_t targetNumber) -> std::string;

}  // namespace Dss::Tracking::GeoDetail
