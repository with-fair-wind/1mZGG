#pragma once

#include <span>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Tracking {

class ITrackingStrategy {
public:
    virtual ~ITrackingStrategy() = default;

    virtual auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> = 0;
    [[nodiscard]] virtual auto mode() const -> Dss::Core::TrackMode = 0;
    virtual void reset() = 0;
};

}  // namespace Dss::Tracking
