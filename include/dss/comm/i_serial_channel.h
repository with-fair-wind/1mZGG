#pragma once

#include <cstddef>
#include <expected>
#include <string>

#include "dss/core/config_types.h"
#include "dss/core/constants.h"

namespace Dss::Comm {

using SerialConfig = Dss::Core::SerialConfig;  ///< 串口配置类型别名

/// 串口通道抽象接口，定义打开/关闭及帧尺寸查询能力
class ISerialChannel {
public:
    virtual ~ISerialChannel() = default;

    /**
     * @brief 打开串口
     * @param config 串口配置（端口名、波特率等）
     * @return 成功返回空值，失败返回错误描述
     */
    virtual auto open(const SerialConfig& config) -> std::expected<void, std::string> = 0;

    /// 关闭串口并释放资源
    virtual void close() = 0;

    /** @brief 查询串口是否已打开。 @return 串口可收发数据时返回 true。 */
    [[nodiscard]] virtual bool isOpen() const = 0;

    /** @brief 获取通道运行状态。 @return 当前生命周期状态。 */
    [[nodiscard]] virtual auto status() const -> Dss::Core::Status = 0;

    /** @brief 获取接收帧长度。 @return 协议定义的固定字节数。 */
    [[nodiscard]] virtual auto recvFrameSize() const -> size_t = 0;

    /** @brief 获取发送帧长度。 @return 协议定义的固定字节数。 */
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t = 0;
};

}  // namespace Dss::Comm
