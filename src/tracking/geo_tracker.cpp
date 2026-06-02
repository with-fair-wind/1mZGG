#include "dss/tracking/geo_tracker.h"

namespace Dss::Tracking {

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
        assoc4();
        findTargets();
    } else {
        trackTargets();
    }

    return m_targets;
}

void GeoTracker::reset() {
    m_fifoTarget.clear();
    m_fifoStar.clear();
    m_targets.clear();
    m_starSpeed = {};
    m_starSpeedAe = {};
    m_targetFound = false;
    m_targetVerified = false;
    m_frameSeq = 0;
}

int GeoTracker::calcStarSpeed() {
    // TODO: port from TrackAlgo::CalcStarSpd()
    return 0;
}

int GeoTracker::assoc4() {
    // TODO: port from TrackAlgo::GEO_Assoc4()
    return 0;
}

int GeoTracker::findTargets() {
    // TODO: port from TrackAlgo::GEO_FindTargets()
    return 0;
}

int GeoTracker::trackTargets() {
    // TODO: port from TrackAlgo::GEO_TrackTargets()
    return 0;
}

}  // namespace Dss::Tracking
