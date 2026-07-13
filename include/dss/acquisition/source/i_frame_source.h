#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <stop_token>
#include <string>

#include "dss/core/types.h"
#include "dss/processing/frame/frame_packet.h"

namespace Dss::Acquisition {

/// @brief 帧源向处理管线提交帧时采用的背压策略。
enum class FrameDeliveryPolicy {
    DropIfBusy,  ///< 实时采集：处理队列繁忙时允许丢弃当前帧。
    Lossless,    ///< 离线回放：处理队列繁忙时等待，保证帧按序入队。
};

/// @brief 单次帧提交所需的策略和取消上下文。
struct FrameDeliveryContext {
    FrameDeliveryPolicy policy = FrameDeliveryPolicy::DropIfBusy;  ///< 当前帧提交策略。
    std::stop_token stopToken;  ///< 用于取消无损提交等待的停止令牌。
};

/// 帧源接口，定义图像采集的抽象层
class IFrameSource {
public:
    /// 帧到达时的回调函数类型；返回 true 表示帧已被下游接收。
    using FrameCallback = std::function<bool(Dss::Processing::FramePacket, FrameDeliveryContext)>;

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

    /** @brief 查询帧源是否正在运行。 @return 正在连续采集或回放时返回 true。 */
    [[nodiscard]] virtual bool isRunning() const = 0;

    /** @brief 获取当前帧宽度。 @return 帧宽度，单位为像素；未知时返回 0。 */
    [[nodiscard]] virtual auto frameWidth() const -> uint32_t = 0;

    /** @brief 获取当前帧高度。 @return 帧高度，单位为像素；未知时返回 0。 */
    [[nodiscard]] virtual auto frameHeight() const -> uint32_t = 0;
};

}  // namespace Dss::Acquisition
