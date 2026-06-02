#pragma once

#include <deque>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

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
    int trackTargets();

    Dss::Core::TrackingSettings m_settings;
    std::deque<Dss::Core::FrameMeasurements> m_fifoTarget;
    std::deque<Dss::Core::FrameMeasurements> m_fifoStar;
    std::vector<Dss::Core::TargetInfo> m_targets;
    Dss::Core::Vec2f m_starSpeed{};
    Dss::Core::Vec2f m_starSpeedAe{};
    bool m_targetFound = false;
    bool m_targetVerified = false;
    uint64_t m_frameSeq = 0;
};

}  // namespace Dss::Tracking
