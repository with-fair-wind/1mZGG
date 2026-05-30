#pragma once

#include "dss/core/constants.h"
#include "dss/core/types.h"
#include "dss/tracking/i_tracking_strategy.h"

#include <memory>
#include <mutex>

namespace Dss::Tracking
{

class TrackManager
{
public:
    TrackManager() = default;

    void setStrategy(std::unique_ptr<ITrackingStrategy> strategy);
    void setMode(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings);

    auto track(const Dss::Core::FrameMeasurements& measurements) -> std::vector<Dss::Core::TargetInfo>;

    [[nodiscard]] auto currentMode() const -> Dss::Core::TrackMode;
    void reset();

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<ITrackingStrategy> m_strategy;
};

} // namespace Dss::Tracking
