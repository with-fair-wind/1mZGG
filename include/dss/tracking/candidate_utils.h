#pragma once

#include <cstddef>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Tracking {

struct InitialMeasurementDedupRule {
    std::size_t frameCount = 0U;
};

[[nodiscard]] bool sharesInitialCentroidAtSameFrameIndex(const Core::TargetInfo& first,
                                                         const Core::TargetInfo& second,
                                                         const InitialMeasurementDedupRule& rule);

[[nodiscard]] auto deduplicateInitialCandidatesByCentroid(std::vector<Core::TargetInfo> candidates,
                                                          const InitialMeasurementDedupRule& rule)
    -> std::vector<Core::TargetInfo>;

}  // namespace Dss::Tracking
