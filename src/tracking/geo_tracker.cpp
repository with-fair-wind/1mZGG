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

namespace Dss::Tracking {

namespace {

inline constexpr int kReliableStarMatchCount = 5;
inline constexpr float kDefaultGeoAssociationRadius = 8.0F;
inline constexpr float kHighConfidenceGeoAssociationRadius = 30.0F;
inline constexpr float kDenseFrameAssociationRadius = 8.0F;
inline constexpr float kGeoMotionConsistencyThreshold = 2.5F;
inline constexpr float kStarLikeSpeedThreshold = 3.0F;
inline constexpr float kAngleAwayFromStarDegrees = 5.0F;
inline constexpr float kSoftDenominatorOffset = 0.001F;
inline constexpr double kGeoSameEquatorialThresholdDeg = 0.0015;
inline constexpr double kTinyCos = 1.0e-8;

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
                                 const Dss::Core::MeasuredBlob& blob)
    -> Dss::Core::TargetFrameInfo {
    Dss::Core::TargetFrameInfo info{};
    info.timestamp = frame.timestamp;
    info.frameSeq = frame.frameSeq;
    info.measuredBlob = blob;
    info.posZxdw = blob.posAe;
    info.posTwdw = blob.posAe;
    info.valid = true;
    return info;
}

void updatePredictionFromHistory(Dss::Core::TargetInfo& target, float frameFrequency) {
    if (target.frameInfos.empty()) {
        return;
    }

    const auto period = frameFrequency > 0.0F ? 1.0F / frameFrequency : 1.0F;
    const auto& latest = target.frameInfos.back().measuredBlob;

    if (target.frameInfos.size() < 2) {
        target.predictedSpdFrame = {};
        target.predictedSpdAe = {};
        target.predictedPosFrame = latest.centroid;
        target.predictedPosAe = latest.posAe;
        return;
    }

    const auto lastIndex = target.frameInfos.size() - 1U;
    const auto motionAt = [&](std::size_t index) {
        const auto& current = target.frameInfos[index].measuredBlob;
        const auto& previous = target.frameInfos[index - 1U].measuredBlob;
        return Dss::Core::Vec2f{current.centroid.x - previous.centroid.x,
                                current.centroid.y - previous.centroid.y};
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
    target.predictedPosFrame =
        Dss::Core::Vec2f{latest.centroid.x + frameMotion.x, latest.centroid.y + frameMotion.y};
    target.predictedSpdAe = Dss::Core::Vec2f{aeMotion.x / period, aeMotion.y / period};
    target.predictedPosAe = Dss::Core::Vec2f{latest.posAe.x + target.predictedSpdAe.x * period,
                                             latest.posAe.y + target.predictedSpdAe.y * period};
    target.lastRmDm = Dss::Core::Vec2f{latest.distAzi, latest.distEle};
}

[[nodiscard]] auto makeAssociatedTarget(
    const std::array<const Dss::Core::FrameMeasurements*, 4>& frames,
    const std::array<const Dss::Core::MeasuredBlob*, 4>& blobs, float frameFrequency)
    -> Dss::Core::TargetInfo {
    Dss::Core::TargetInfo target{};
    target.targetId = "geo";
    target.validity = 1.0F;
    target.living = true;

    for (std::size_t index = 0; index < frames.size(); ++index) {
        target.frameInfos.push_back(makeFrameInfo(*frames[index], *blobs[index]));
    }
    updatePredictionFromHistory(target, frameFrequency);
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

[[nodiscard]] bool sharesInitialMeasurements(const Dss::Core::TargetInfo& first,
                                             const Dss::Core::TargetInfo& second) {
    if (first.frameInfos.size() < 3 || second.frameInfos.size() < 3) {
        return false;
    }
    for (std::size_t index = 0; index < 3; ++index) {
        const auto& firstBlob = first.frameInfos[index].measuredBlob;
        const auto& secondBlob = second.frameInfos[index].measuredBlob;
        if (firstBlob.centroid.x == secondBlob.centroid.x &&
            firstBlob.centroid.y == secondBlob.centroid.y) {
            return true;
        }
    }
    return false;
}

void removeDuplicateAssociations(std::vector<Dss::Core::TargetInfo>& targets) {
    std::vector<std::size_t> removeIndexes;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        for (std::size_t j = i + 1U; j < targets.size(); ++j) {
            if (sharesInitialMeasurements(targets[i], targets[j])) {
                removeIndexes.push_back(j);
            }
        }
    }

    std::ranges::sort(removeIndexes, std::greater<>{});
    removeIndexes.erase(std::ranges::unique(removeIndexes).begin(), removeIndexes.end());
    for (const auto index : removeIndexes) {
        if (index < targets.size()) {
            targets.erase(targets.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
}

[[nodiscard]] bool hasSameCentroid(const Dss::Core::MeasuredBlob& first,
                                   const Dss::Core::MeasuredBlob& second) {
    return first.centroid.x == second.centroid.x && first.centroid.y == second.centroid.y;
}

[[nodiscard]] bool isAlreadyUsed(const Dss::Core::MeasuredBlob& candidate,
                                 const std::vector<Dss::Core::MeasuredBlob>& usedBlobs) {
    return std::any_of(
        usedBlobs.begin(), usedBlobs.end(),
        [&](const Dss::Core::MeasuredBlob& used) { return hasSameCentroid(candidate, used); });
}

[[nodiscard]] auto findNearestBlob(const std::vector<Dss::Core::MeasuredBlob>& blobs,
                                   const Dss::Core::Vec2f& predicted, float radius,
                                   const std::vector<Dss::Core::MeasuredBlob>& usedBlobs)
    -> std::optional<Dss::Core::MeasuredBlob> {
    const auto radiusSquared = radius * radius;
    auto bestDistance = std::numeric_limits<float>::max();
    std::optional<Dss::Core::MeasuredBlob> bestBlob;
    for (const auto& blob : blobs) {
        if (isAlreadyUsed(blob, usedBlobs)) {
            continue;
        }
        const auto dx = blob.centroid.x - predicted.x;
        const auto dy = blob.centroid.y - predicted.y;
        const auto distance = dx * dx + dy * dy;
        if (distance < radiusSquared && distance < bestDistance) {
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

[[nodiscard]] bool latestFramesAreAllInvalid(const Dss::Core::TargetInfo& target, int frameWindow) {
    if (frameWindow <= 0 || target.frameInfos.empty()) {
        return false;
    }

    const auto checkCount =
        std::min(target.frameInfos.size(), static_cast<std::size_t>(frameWindow));
    return std::all_of(target.frameInfos.end() - static_cast<std::ptrdiff_t>(checkCount),
                       target.frameInfos.end(),
                       [](const Dss::Core::TargetFrameInfo& info) { return !info.valid; });
}

[[nodiscard]] bool hasLivingTarget(const std::vector<Dss::Core::TargetInfo>& targets) {
    return std::ranges::any_of(targets,
                               [](const Dss::Core::TargetInfo& target) { return target.living; });
}

}  // namespace

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
}

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

    const auto radius = associationRadius(m_settings, m_starMatchCount, frame3.targetBlobs.size());
    std::vector<Dss::Core::TargetInfo> associatedTargets;

    for (const auto& blob0 : frame0.targetBlobs) {
        for (const auto& blob1 : frame1.targetBlobs) {
            const auto motion10 = Dss::Core::Vec2f{blob1.centroid.x - blob0.centroid.x,
                                                   blob1.centroid.y - blob0.centroid.y};
            if (std::abs(motion10.x) >= radius || std::abs(motion10.y) >= radius ||
                isMotionStarLike(motion10, m_starSpeed) ||
                !isMotionAngleAwayFromStars(motion10, m_starSpeed)) {
                continue;
            }

            for (const auto& blob2 : frame2.targetBlobs) {
                const auto motion21 = Dss::Core::Vec2f{blob2.centroid.x - blob1.centroid.x,
                                                       blob2.centroid.y - blob1.centroid.y};
                if (std::abs(motion21.x) >= radius || std::abs(motion21.y) >= radius ||
                    isMotionStarLike(motion21, m_starSpeed) ||
                    std::abs(motion21.x - motion10.x) > kGeoMotionConsistencyThreshold ||
                    std::abs(motion21.y - motion10.y) > kGeoMotionConsistencyThreshold ||
                    !isMotionAngleAwayFromStars(motion21, m_starSpeed)) {
                    continue;
                }

                for (const auto& blob3 : frame3.targetBlobs) {
                    const auto motion32 = Dss::Core::Vec2f{blob3.centroid.x - blob2.centroid.x,
                                                           blob3.centroid.y - blob2.centroid.y};
                    if (std::abs(motion32.x) >= radius || std::abs(motion32.y) >= radius ||
                        isMotionStarLike(motion32, m_starSpeed) ||
                        std::abs(motion32.x - motion21.x) > kGeoMotionConsistencyThreshold ||
                        std::abs(motion32.y - motion21.y) > kGeoMotionConsistencyThreshold ||
                        std::abs(motion32.x - motion10.x) > kGeoMotionConsistencyThreshold ||
                        std::abs(motion32.y - motion10.y) > kGeoMotionConsistencyThreshold ||
                        !isMotionAngleAwayFromStars(motion32, m_starSpeed)) {
                        continue;
                    }

                    const std::array blobs{&blob0, &blob1, &blob2, &blob3};
                    if (hasRepeatedEquatorialPoint(blobs)) {
                        continue;
                    }
                    const std::array frames{&frame0, &frame1, &frame2, &frame3};
                    associatedTargets.push_back(makeAssociatedTarget(frames, blobs, m_frameFreq));
                }
            }
        }
    }

    removeDuplicateAssociations(associatedTargets);
    m_targets = std::move(associatedTargets);
    return 0;
}

int GeoTracker::findTargets() {
    m_targetFound = !m_targets.empty();
    m_targetVerified = m_targetFound;
    return m_targetFound ? 0 : 1;
}

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

        const auto matchedBlob = findNearestBlob(frame.targetBlobs, target.predictedPosFrame,
                                                 m_settings.searchRadius, usedBlobs);
        if (!matchedBlob.has_value()) {
            Dss::Core::MeasuredBlob predictedBlob{};
            predictedBlob.id = target.targetId;
            predictedBlob.centroid = target.predictedPosFrame;
            predictedBlob.posAe = target.predictedPosAe;
            auto info = makeFrameInfo(frame, predictedBlob);
            info.valid = false;
            target.frameInfos.push_back(info);
        } else {
            target.frameInfos.push_back(makeFrameInfo(frame, *matchedBlob));
            usedBlobs.push_back(*matchedBlob);
        }

        updatePredictionFromHistory(target, m_frameFreq);
        const auto sampleCount = static_cast<float>(target.frameInfos.size());
        const auto latestValid = target.frameInfos.back().valid ? 1.0F : 0.0F;
        target.validity = ((sampleCount - 1.0F) * target.validity + latestValid) / sampleCount;
        target.living = target.living &&
                        isInsideImageBounds(target.predictedPosFrame, m_settings.opticParams) &&
                        !latestFramesAreAllInvalid(target, m_settings.numFramesLiving);
    }
    return 0;
}

}  // namespace Dss::Tracking
