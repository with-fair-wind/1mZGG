#include "dss/tracking/track_manager.h"

#include <memory>

#include "dss/tracking/geo_tracker.h"
#include "dss/tracking/leo_tracker.h"
#include "dss/tracking/manual_tracker.h"
#include "dss/tracking/sc_tracker.h"

namespace Dss::Tracking {

/// 按跟踪模式工厂方法创建 Geo/SC/LEO/Manual 策略实例
auto makeTrackingStrategy(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings)
    -> std::unique_ptr<ITrackingStrategy> {
    switch (mode) {
        case Dss::Core::TrackMode::Geo:
            return std::make_unique<GeoTracker>(settings);
        case Dss::Core::TrackMode::SpaceCatalog:
            return std::make_unique<ScTracker>(settings);
        case Dss::Core::TrackMode::Leo:
            return std::make_unique<LeoTracker>(settings);
        case Dss::Core::TrackMode::Manual:
            return std::make_unique<ManualTracker>(settings);
        case Dss::Core::TrackMode::Init:
            return nullptr;
    }
    return nullptr;
}

void TrackManager::setStrategy(std::unique_ptr<ITrackingStrategy> strategy) {
    std::lock_guard lock(m_mutex);
    m_strategy = std::move(strategy);
}

void TrackManager::setMode(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings) {
    setStrategy(makeTrackingStrategy(mode, settings));
}

auto TrackManager::track(const Dss::Core::FrameMeasurements& measurements)
    -> std::vector<Dss::Core::TargetInfo> {
    std::lock_guard lock(m_mutex);
    if (!m_strategy) {
        return {};
    }
    return m_strategy->track(measurements);
}

auto TrackManager::currentMode() const -> Dss::Core::TrackMode {
    std::lock_guard lock(m_mutex);
    return m_strategy ? m_strategy->mode() : Dss::Core::TrackMode::Init;
}

void TrackManager::reset() {
    std::lock_guard lock(m_mutex);
    if (m_strategy) {
        m_strategy->reset();
    }
}

}  // namespace Dss::Tracking
