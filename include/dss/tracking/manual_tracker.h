#pragma once

#include <deque>
#include <mutex>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

/// 手动跟踪器，根据操作员指定的目标位置生成跟踪轨迹
class ManualTracker final : public ITrackingStrategy {
public:
    explicit ManualTracker(const Dss::Core::TrackingSettings& settings);

    /**
     * @brief 处理单帧测量，基于手动指定目标更新轨迹与预测
     * @param measurements 当前帧测量数据
     * @return 包含手动目标的列表；未设置手动目标时返回空
     */
    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;

    /// 返回手动跟踪模式
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Manual;
    }

    /// 重置 FIFO 缓存、目标状态与手动目标
    void reset() override;

    /**
     * @brief 设置操作员指定的手动跟踪目标
     * @param blob 目标像斑测量（至少包含质心坐标）
     */
    void setManualTarget(const Dss::Core::MeasuredBlob& blob);

private:
    Dss::Core::TrackingSettings m_settings;           ///< 跟踪算法参数
    std::deque<Dss::Core::FrameMeasurements> m_fifo;  ///< 历史帧 FIFO
    Dss::Core::TargetInfo m_currentTarget{};          ///< 当前手动跟踪目标
    Dss::Core::MeasuredBlob m_manualBlob{};           ///< 操作员指定的目标像斑
    bool m_hasManualBlob = false;                     ///< 是否已设置手动目标
    std::mutex m_blobMutex;                           ///< 保护手动目标数据的互斥锁
};

}  // namespace Dss::Tracking
