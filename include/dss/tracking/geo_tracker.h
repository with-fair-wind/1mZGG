#pragma once

#include <deque>
#include <span>
#include <vector>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

enum class GeoStarSpeedStatus {
    Ok,
    InsufficientFrames,
    MissingStars,
    NoCandidates,
};

struct GeoStarSpeedResult {
    GeoStarSpeedStatus status = GeoStarSpeedStatus::InsufficientFrames;
    Dss::Core::Vec2f frameSpeed{};
    Dss::Core::Vec2f aeSpeed{};
    int matchCount = 0;
};

[[nodiscard]] auto estimateGeoStarSpeed(std::span<const Dss::Core::FrameMeasurements> frames,
                                        float ratioFov, float radius,
                                        const Dss::Core::OpticParams& opticParams)
    -> GeoStarSpeedResult;

class GeoTracker final : public ITrackingStrategy {
public:
    explicit GeoTracker(const Dss::Core::TrackingSettings& settings);

    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Geo;
    }
    void reset() override;

private:
    int calcStarSpeed();
    int assoc4();
    int findTargets();
    int refindTargets();
    int trackTargets();
    void assignTargetIds(std::vector<Dss::Core::TargetInfo>& targets);

    Dss::Core::TrackingSettings m_settings;
    std::deque<Dss::Core::FrameMeasurements> m_fifoTarget;
    std::deque<Dss::Core::FrameMeasurements> m_fifoStar;
    std::vector<Dss::Core::TargetInfo> m_targets;
    Dss::Core::Vec2f m_starSpeed{};
    Dss::Core::Vec2f m_starSpeedAe{};
    float m_frameFreq = 1.0f;
    int m_starMatchCount = 0;
    bool m_targetFound = false;
    bool m_targetVerified = false;
    uint64_t m_frameSeq = 0;
    uint64_t m_nextTargetId = 1;
};

}  // namespace Dss::Tracking
