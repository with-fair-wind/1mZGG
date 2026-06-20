#include "dss/tracking/prediction_utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

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

auto findValidatedBlobForTarget(std::span<const Core::MeasuredBlob> validatedBlobs,
                                std::string_view targetId) -> const Core::MeasuredBlob* {
    const auto match = std::ranges::find_if(
        validatedBlobs, [targetId](const Core::MeasuredBlob& blob) { return blob.id == targetId; });
    return match == validatedBlobs.end() ? nullptr : std::addressof(*match);
}

auto makeInvalidFallbackBlob(const Core::FrameMeasurements& frame, const Core::TargetInfo& target,
                             const InvalidFallbackBlobOptions& options) -> Core::MeasuredBlob {
    if (const auto* validatedBlob =
            findValidatedBlobForTarget(frame.validatedTargetBlobs, target.targetId);
        validatedBlob != nullptr) {
        return *validatedBlob;
    }

    Core::MeasuredBlob predictedBlob{};
    predictedBlob.id = target.targetId;
    predictedBlob.centroid = target.predictedPosFrame;
    predictedBlob.maxX = predictedBlob.centroid.x + options.halfExtent;
    predictedBlob.minX = predictedBlob.centroid.x - options.halfExtent;
    predictedBlob.maxY = predictedBlob.centroid.y + options.halfExtent;
    predictedBlob.minY = predictedBlob.centroid.y - options.halfExtent;
    predictedBlob.area = 0.0F;
    predictedBlob.dn = 0.0F;
    predictedBlob.posAe = target.predictedPosAe;
    if (options.copyPredictedFrameToRaDec) {
        predictedBlob.ra = target.predictedPosFrame.x;
        predictedBlob.dec = target.predictedPosFrame.y;
    }
    return predictedBlob;
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
    info.posZxdw = (blob.targetAzi != 0.0F || blob.targetEle != 0.0F)
                       ? Core::Vec2f{blob.targetAzi, blob.targetEle}
                       : blob.posAe;
    info.posTwdw = (blob.ra != 0.0 || blob.dec != 0.0)
                       ? Core::Vec2f{static_cast<float>(blob.ra), static_cast<float>(blob.dec)}
                       : blob.posAe;
    info.magnitude = blob.magnitude;
    info.valid = true;
    return info;
}

/// 构造未匹配帧的占位信息，使用预测位置填充像斑字段
auto makeInvalidTargetFrameInfo(const Core::FrameMeasurements& frame,
                                const Core::TargetInfo& target,
                                const Core::TrackingSettings& settings, float halfExtent)
    -> Core::TargetFrameInfo {
    InvalidFallbackBlobOptions options{};
    options.halfExtent = halfExtent;
    const auto fallbackBlob = makeInvalidFallbackBlob(frame, target, options);

    auto info = makeTargetFrameInfo(frame, fallbackBlob, settings);
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

/// 在阈值门限内搜索距预测位置最近的像斑，可选视场中心约束
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

auto appendMatchedFrameAndUpdatePrediction(const Core::FrameMeasurements& frame,
                                           const Core::TargetInfo& target,
                                           const Core::TrackingSettings& settings,
                                           const BlobMatchOptions& options, float invalidHalfExtent)
    -> Core::TargetInfo {
    auto updated = target;
    const auto* matchedBlob = findNearestBlob(frame, updated, settings, options);
    updated.frameInfos.push_back(
        matchedBlob == nullptr
            ? makeInvalidTargetFrameInfo(frame, updated, settings, invalidHalfExtent)
            : makeTargetFrameInfo(frame, *matchedBlob, settings));
    updatePredictionFromRecentFour(updated);
    return updated;
}

/// 取最近三帧像面/AE 运动的中位数更新预测，并滚动更新有效性
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
