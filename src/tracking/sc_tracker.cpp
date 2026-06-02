#include "dss/tracking/sc_tracker.h"

namespace Dss::Tracking {

ScTracker::ScTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

auto ScTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifo.push_back(measurements);
    if (m_fifo.size() > 10) {
        m_fifo.pop_front();
    }

    // TODO: port from TrackAlgo::TrackProc_SC()

    return {m_currentTarget};
}

void ScTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
}

}  // namespace Dss::Tracking
