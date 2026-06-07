#pragma once

#include <deque>
#include <vector>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

class ScTracker final : public ITrackingStrategy {
public:
    explicit ScTracker(const Dss::Core::TrackingSettings& settings);

    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::SpaceCatalog;
    }
    void reset() override;

private:
    Dss::Core::TrackingSettings m_settings;
    std::deque<Dss::Core::FrameMeasurements> m_fifo;
    Dss::Core::TargetInfo m_currentTarget{};
    std::vector<Dss::Core::TargetInfo> m_candidates;
    bool m_targetFound = false;
};

}  // namespace Dss::Tracking
