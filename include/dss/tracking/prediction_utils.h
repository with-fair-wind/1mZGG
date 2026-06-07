#pragma once

#include <cstddef>

#include "dss/core/types.h"

namespace Dss::Tracking {

enum class BlobMatchSpace {
    Frame,
    Ae,
};

struct BlobMatchOptions {
    BlobMatchSpace space = BlobMatchSpace::Frame;
    float threshold = 0.0F;
    bool requireFovCenter = false;
    float fovCenterHalfExtent = 128.0F;
};

[[nodiscard]] auto median3(float first, float second, float third) -> float;

[[nodiscard]] auto makeTargetFrameInfo(const Core::FrameMeasurements& frame,
                                       const Core::MeasuredBlob& blob,
                                       const Core::TrackingSettings& settings)
    -> Core::TargetFrameInfo;

[[nodiscard]] auto makeInvalidTargetFrameInfo(const Core::FrameMeasurements& frame,
                                              const Core::TargetInfo& target,
                                              const Core::TrackingSettings& settings,
                                              float halfExtent = 5.0F) -> Core::TargetFrameInfo;

[[nodiscard]] auto framePeriodSeconds(const Core::FrameMeasurements& frame) -> float;

[[nodiscard]] auto framePeriodSeconds(const Core::TargetFrameInfo& frame) -> float;

[[nodiscard]] auto frameMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to)
    -> Core::Vec2f;

[[nodiscard]] auto aeMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to)
    -> Core::Vec2f;

[[nodiscard]] auto targetFrameMotionAt(const Core::TargetInfo& target, std::size_t index)
    -> Core::Vec2f;

[[nodiscard]] auto targetAeMotionAt(const Core::TargetInfo& target, std::size_t index)
    -> Core::Vec2f;

[[nodiscard]] bool isNearFovCenter(const Core::MeasuredBlob& blob,
                                   const Core::TrackingSettings& settings, float halfExtent);

[[nodiscard]] auto findNearestBlob(const Core::FrameMeasurements& frame,
                                   const Core::TargetInfo& target,
                                   const Core::TrackingSettings& settings,
                                   const BlobMatchOptions& options) -> const Core::MeasuredBlob*;

void updatePredictionFromRecentFour(Core::TargetInfo& target);

}  // namespace Dss::Tracking
