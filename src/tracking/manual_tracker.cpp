#include "dss/tracking/manual_tracker.h"

namespace Dss::Tracking {

ManualTracker::ManualTracker(const Dss::Core::TrackingSettings& settings) : m_settings(settings) {}

auto ManualTracker::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    m_fifo.push_back(measurements);
    if (m_fifo.size() > 10) {
        m_fifo.pop_front();
    }

    // TODO: port from TrackAlgo::TrackProc_MANUAL()
    Dss::Core::MeasuredBlob blob;
    {
        std::lock_guard lock(m_blobMutex);
        blob = m_manualBlob;
    }
    (void)blob;

    return {m_currentTarget};
}

void ManualTracker::reset() {
    m_fifo.clear();
    m_currentTarget = {};
}

void ManualTracker::setManualTarget(const Dss::Core::MeasuredBlob& blob) {
    std::lock_guard lock(m_blobMutex);
    m_manualBlob = blob;
}

}  // namespace Dss::Tracking
