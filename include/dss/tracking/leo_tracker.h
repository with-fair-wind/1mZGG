#pragma once

#include <deque>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

/// 低轨（LEO）目标跟踪器，基于三帧方位-俯仰运动关联
class LeoTracker final : public ITrackingStrategy {
public:
    /**
     * @brief 创建低轨目标跟踪器。
     * @param settings 跟踪门限、光学参数和生命周期配置。
     */
    explicit LeoTracker(const Dss::Core::TrackingSettings& settings);

    /**
     * @brief 处理单帧测量，执行三帧关联、验证与跟踪
     * @param measurements 当前帧测量数据
     * @return 当前活跃目标列表（验证前返回候选，验证后返回选定目标）
     */
    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;

    /** @brief 获取策略模式。 @return 固定返回 TrackMode::Leo。 */
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Leo;
    }

    /// 重置 FIFO 缓存与目标状态
    void reset() override;

private:
    Dss::Core::TrackingSettings m_settings;           ///< 跟踪算法参数
    std::deque<Dss::Core::FrameMeasurements> m_fifo;  ///< 最近三帧测量 FIFO
    Dss::Core::TargetInfo m_currentTarget{};          ///< 当前选定的跟踪目标
    std::vector<Dss::Core::TargetInfo> m_candidates;  ///< 三帧关联产生的候选目标
    bool m_targetFound = false;                       ///< 是否已发现候选目标
    bool m_targetVerified = false;                    ///< 候选目标是否已通过验证
};

}  // namespace Dss::Tracking
