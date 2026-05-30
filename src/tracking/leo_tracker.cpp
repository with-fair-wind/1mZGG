#include "dss/tracking/leo_tracker.h"

namespace Dss::Tracking
{

LeoTracker::LeoTracker(const Dss::Core::TrackingSettings& settings)
    : m_settings(settings)
{
}

auto LeoTracker::track(const Dss::Core::FrameMeasurements& measurements) -> std::vector<Dss::Core::TargetInfo>
{
    m_fifo.push_back(measurements);
    if (m_fifo.size() > 10)
    {
        m_fifo.pop_front();
    }

    // TODO: port from TrackAlgo::TrackProc_LEO()

    return {m_currentTarget};
}

void LeoTracker::reset()
{
    m_fifo.clear();
    m_currentTarget = {};
    m_candidates.clear();
    m_targetFound = false;
}

} // namespace Dss::Tracking
