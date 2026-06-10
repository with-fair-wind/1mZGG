#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

#include "dss/core/types.h"
#include "dss/processing/frame_packet.h"

namespace Dss::Acquisition {

/// 帧源接口，定义图像采集的抽象层
class IFrameSource {
public:
    /// 帧到达时的回调函数类型
    using FrameCallback = std::function<void(Dss::Processing::FramePacket)>;

    virtual ~IFrameSource() = default;

    /**
     * @brief 初始化帧源
     * @return 失败时返回包含错误信息的 std::expected
     */
    virtual auto init() -> std::expected<void, std::string> = 0;

    /// 开始连续采集或回放
    virtual void start() = 0;

    /// 停止采集并释放后台资源
    virtual void stop() = 0;

    /**
     * @brief 设置帧回调函数
     * @param callback 每帧就绪时调用的回调
     */
    virtual void setFrameCallback(FrameCallback callback) = 0;

    /// 当前是否处于运行状态
    [[nodiscard]] virtual bool isRunning() const = 0;

    /// 帧宽度（像素）
    [[nodiscard]] virtual auto frameWidth() const -> uint32_t = 0;

    /// 帧高度（像素）
    [[nodiscard]] virtual auto frameHeight() const -> uint32_t = 0;
};

}  // namespace Dss::Acquisition
