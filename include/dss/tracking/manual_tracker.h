#pragma once

#include <deque>
#include <mutex>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

class ManualTracker final : public ITrackingStrategy {
public:
    explicit ManualTracker(const Dss::Core::TrackingSettings& settings);

    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Manual;
    }
    void reset() override;

    void setManualTarget(const Dss::Core::MeasuredBlob& blob);

private:
    Dss::Core::TrackingSettings m_settings;
    std::deque<Dss::Core::FrameMeasurements> m_fifo;
    Dss::Core::TargetInfo m_currentTarget{};
    Dss::Core::MeasuredBlob m_manualBlob{};
    std::mutex m_blobMutex;
};

}  // namespace Dss::Tracking
