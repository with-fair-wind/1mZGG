#pragma once

#include <memory>
#include <mutex>

#include "dss/core/constants.h"
#include "dss/core/types.h"
#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

/**
 * @brief 根据跟踪模式创建对应的跟踪策略实例
 * @param mode 目标跟踪模式
 * @param settings 跟踪算法参数
 * @return 策略实例；Init 模式返回 nullptr
 */
[[nodiscard]] auto makeTrackingStrategy(Dss::Core::TrackMode mode,
                                        const Dss::Core::TrackingSettings& settings)
    -> std::unique_ptr<ITrackingStrategy>;

/// 跟踪管理器，负责策略切换与线程安全的跟踪调度
class TrackManager {
public:
    TrackManager() = default;

    /**
     * @brief 替换当前跟踪策略。
     * @param strategy 新策略的独占所有权；传入空指针可禁用跟踪。
     */
    void setStrategy(std::unique_ptr<ITrackingStrategy> strategy);

    /**
     * @brief 按模式创建并设置跟踪策略
     * @param mode 目标跟踪模式
     * @param settings 跟踪算法参数
     */
    void setMode(Dss::Core::TrackMode mode, const Dss::Core::TrackingSettings& settings);

    /**
     * @brief 将当前帧测量交给策略处理
     * @param measurements 当前帧测量数据
     * @return 策略返回的目标列表；无策略时返回空
     */
    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo>;

    /**
     * @brief 获取当前跟踪模式。
     * @return 当前策略模式；无策略时返回 TrackMode::Init。
     */
    [[nodiscard]] auto currentMode() const -> Dss::Core::TrackMode;

    /// 重置当前策略的内部状态
    void reset();

private:
    mutable std::mutex m_mutex;                     ///< 保护策略访问的互斥锁
    std::unique_ptr<ITrackingStrategy> m_strategy;  ///< 当前活跃的跟踪策略
};

}  // namespace Dss::Tracking
