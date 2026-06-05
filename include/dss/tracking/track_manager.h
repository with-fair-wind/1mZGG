#pragma once

#include <memory>
#include <mutex>

#include "dss/core/constants.h"
#include "dss/core/types.h"
#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

[[nodiscard]] auto makeTrackingStrategy(Dss::Core::TrackMode mode,
                                        const Dss::Core::TrackingSettings& settings)
    -> std::unique_ptr<ITrackingStrategy>;

class TrackManager {
public:
    TrackManager() = default;

    void setStrategy(std::unique_ptr<ITrackingStrategy> strategy);
    void setMode(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings);

    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo>;

    [[nodiscard]] auto currentMode() const -> Dss::Core::TrackMode;
    void reset();

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<ITrackingStrategy> m_strategy;
};

}  // namespace Dss::Tracking
