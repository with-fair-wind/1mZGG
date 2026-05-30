#include "dss/tracking/track_manager.h"

namespace Dss::Tracking
{

void TrackManager::setStrategy(std::unique_ptr<ITrackingStrategy> strategy)
{
    std::lock_guard lock(m_mutex);
    m_strategy = std::move(strategy);
}

void TrackManager::setMode(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings)
{
    // TODO: instantiate appropriate strategy based on mode
    (void)mode;
    (void)settings;
}

auto TrackManager::track(const Dss::Core::FrameMeasurements& measurements) -> std::vector<Dss::Core::TargetInfo>
{
    std::lock_guard lock(m_mutex);
    if (!m_strategy)
    {
        return {};
    }
    return m_strategy->track(measurements);
}

auto TrackManager::currentMode() const -> Dss::Core::TrackMode
{
    std::lock_guard lock(m_mutex);
    return m_strategy ? m_strategy->mode() : Dss::Core::TrackMode::Init;
}

void TrackManager::reset()
{
    std::lock_guard lock(m_mutex);
    if (m_strategy)
    {
        m_strategy->reset();
    }
}

} // namespace Dss::Tracking
