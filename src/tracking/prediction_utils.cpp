#include "dss/tracking/prediction_utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

[[nodiscard]] auto predictedPosition(const Dss::Core::TargetInfo& target,
                                     Dss::Tracking::BlobMatchSpace space) -> Dss::Core::Vec2f {
    return space == Dss::Tracking::BlobMatchSpace::Frame ? target.predictedPosFrame
                                                         : target.predictedPosAe;
}

[[nodiscard]] auto blobPosition(const Dss::Core::MeasuredBlob& blob,
                                Dss::Tracking::BlobMatchSpace space) -> Dss::Core::Vec2f {
    return space == Dss::Tracking::BlobMatchSpace::Frame ? blob.centroid : blob.posAe;
}

}  // namespace

namespace Dss::Tracking {

auto median3(float first, float second, float third) -> float {
    const auto minValue = std::min(first, std::min(second, third));
    const auto maxValue = std::max(first, std::max(second, third));
    return first + second + third - minValue - maxValue;
}

auto makeTargetFrameInfo(const Core::FrameMeasurements& frame, const Core::MeasuredBlob& blob,
                         const Core::TrackingSettings& settings) -> Core::TargetFrameInfo {
    Core::TargetFrameInfo info{};
    info.timestamp = frame.timestamp;
    info.frameSeq = frame.frameSeq;
    info.fovCenterAe = frame.fovCenterAe;
    info.opticCenter =
        Core::Vec2f{settings.opticParams.fovCenterX, settings.opticParams.fovCenterY};
    info.exposureTime = frame.exposureTime;
    info.frameFreq = frame.frameFreq;
    info.measuredBlob = blob;
    info.posZxdw = blob.posAe;
    info.posTwdw = blob.posAe;
    info.valid = true;
    return info;
}

auto makeInvalidTargetFrameInfo(const Core::FrameMeasurements& frame,
                                const Core::TargetInfo& target,
                                const Core::TrackingSettings& settings, float halfExtent)
    -> Core::TargetFrameInfo {
    Core::MeasuredBlob predictedBlob{};
    predictedBlob.id = target.targetId;
    predictedBlob.centroid = target.predictedPosFrame;
    predictedBlob.maxX = predictedBlob.centroid.x + halfExtent;
    predictedBlob.minX = predictedBlob.centroid.x - halfExtent;
    predictedBlob.maxY = predictedBlob.centroid.y + halfExtent;
    predictedBlob.minY = predictedBlob.centroid.y - halfExtent;
    predictedBlob.area = 0.0F;
    predictedBlob.dn = 0.0F;
    predictedBlob.posAe = target.predictedPosAe;

    auto info = makeTargetFrameInfo(frame, predictedBlob, settings);
    info.valid = false;
    return info;
}

auto framePeriodSeconds(const Core::FrameMeasurements& frame) -> float {
    return frame.frameFreq > 0.0F ? 1.0F / frame.frameFreq : 1.0F;
}

auto framePeriodSeconds(const Core::TargetFrameInfo& frame) -> float {
    return frame.frameFreq > 0.0F ? 1.0F / frame.frameFreq : 1.0F;
}

auto frameMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to) -> Core::Vec2f {
    return Core::Vec2f{to.centroid.x - from.centroid.x, to.centroid.y - from.centroid.y};
}

auto aeMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to) -> Core::Vec2f {
    return Core::Vec2f{to.posAe.x - from.posAe.x, to.posAe.y - from.posAe.y};
}

auto targetFrameMotionAt(const Core::TargetInfo& target, std::size_t index) -> Core::Vec2f {
    const auto& current = target.frameInfos[index].measuredBlob;
    const auto& previous = target.frameInfos[index - 1U].measuredBlob;
    return frameMotion(previous, current);
}

auto targetAeMotionAt(const Core::TargetInfo& target, std::size_t index) -> Core::Vec2f {
    const auto& current = target.frameInfos[index].measuredBlob;
    const auto& previous = target.frameInfos[index - 1U].measuredBlob;
    return aeMotion(previous, current);
}

bool isNearFovCenter(const Core::MeasuredBlob& blob, const Core::TrackingSettings& settings,
                     float halfExtent) {
    return std::abs(blob.centroid.x - settings.opticParams.fovCenterX) < halfExtent &&
           std::abs(blob.centroid.y - settings.opticParams.fovCenterY) < halfExtent;
}

auto findNearestBlob(const Core::FrameMeasurements& frame, const Core::TargetInfo& target,
                     const Core::TrackingSettings& settings, const BlobMatchOptions& options)
    -> const Core::MeasuredBlob* {
    const auto predicted = predictedPosition(target, options.space);
    const Core::MeasuredBlob* nearestBlob = nullptr;
    auto nearestDistance = std::numeric_limits<float>::max();
    for (const auto& blob : frame.targetBlobs) {
        if (options.requireFovCenter &&
            !isNearFovCenter(blob, settings, options.fovCenterHalfExtent)) {
            continue;
        }

        const auto actual = blobPosition(blob, options.space);
        const auto dx = predicted.x - actual.x;
        const auto dy = predicted.y - actual.y;
        const auto distance = dx * dx + dy * dy;
        if (std::abs(dx) <= options.threshold && std::abs(dy) <= options.threshold &&
            distance <= nearestDistance) {
            nearestBlob = &blob;
            nearestDistance = distance;
        }
    }
    return nearestBlob;
}

void updatePredictionFromRecentFour(Core::TargetInfo& target) {
    const auto size = target.frameInfos.size();
    if (size < 4U) {
        return;
    }

    const auto frameMotion3 = targetFrameMotionAt(target, size - 1U);
    const auto frameMotion2 = targetFrameMotionAt(target, size - 2U);
    const auto frameMotion1 = targetFrameMotionAt(target, size - 3U);
    const auto aeMotion3 = targetAeMotionAt(target, size - 1U);
    const auto aeMotion2 = targetAeMotionAt(target, size - 2U);
    const auto aeMotion1 = targetAeMotionAt(target, size - 3U);
    const auto& latestBlob = target.frameInfos.back().measuredBlob;
    const auto period = framePeriodSeconds(target.frameInfos.back());

    target.predictedSpdFrame = Core::Vec2f{median3(frameMotion3.x, frameMotion2.x, frameMotion1.x),
                                           median3(frameMotion3.y, frameMotion2.y, frameMotion1.y)};
    target.predictedPosFrame = Core::Vec2f{latestBlob.centroid.x + target.predictedSpdFrame.x,
                                           latestBlob.centroid.y + target.predictedSpdFrame.y};
    target.predictedSpdAe = Core::Vec2f{median3(aeMotion3.x, aeMotion2.x, aeMotion1.x) / period,
                                        median3(aeMotion3.y, aeMotion2.y, aeMotion1.y) / period};
    target.predictedPosAe = Core::Vec2f{latestBlob.posAe.x + target.predictedSpdAe.x * period,
                                        latestBlob.posAe.y + target.predictedSpdAe.y * period};

    const auto latestValid = target.frameInfos.back().valid ? 1.0F : 0.0F;
    target.validity = ((static_cast<float>(size - 1U) * target.validity) + latestValid) /
                      static_cast<float>(size);
}

}  // namespace Dss::Tracking
