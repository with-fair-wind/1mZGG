#pragma once

#include <span>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Tracking {

/// 跟踪策略接口，定义各轨道模式跟踪器的统一行为
class ITrackingStrategy {
public:
    virtual ~ITrackingStrategy() = default;

    /**
     * @brief 处理单帧测量数据并更新目标轨迹
     * @param measurements 当前帧的目标与恒星测量结果
     * @return 当前活跃的目标列表
     */
    virtual auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> = 0;

    /// 返回当前策略对应的跟踪模式
    [[nodiscard]] virtual auto mode() const -> Dss::Core::TrackMode = 0;

    /// 重置内部状态，清除历史帧与目标缓存
    virtual void reset() = 0;
};

}  // namespace Dss::Tracking
