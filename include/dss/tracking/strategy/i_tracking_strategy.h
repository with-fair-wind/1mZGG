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
     * @return 当前目标状态快照，具体策略可能同时返回刚失活的目标。
     * @note frameInfos 是有界近期窗口，不是轨迹归档；累计长度使用 totalFrameCount()。
     * 需要完整轨迹时应逐帧记录最新测量。reset 会清空窗口及累计计数。
     *
     * 空列表表示本帧无目标；GEO 刚失活目标仅返回一次，详情遵循具体策略契约。

     */
    virtual auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> = 0;

    /**
     * @brief 获取当前策略对应的跟踪模式。
     * @return 策略实现所支持的跟踪模式。
     */
    [[nodiscard]] virtual auto mode() const -> Dss::Core::TrackMode = 0;

    /// 重置内部状态，清除历史帧与目标缓存
    virtual void reset() = 0;
};

}  // namespace Dss::Tracking
