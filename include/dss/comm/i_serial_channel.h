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

    /// 查询串口是否已打开
    [[nodiscard]] virtual bool isOpen() const = 0;

    /// 获取当前通道运行状态
    [[nodiscard]] virtual auto status() const -> Dss::Core::Status = 0;

    /// 返回接收帧的固定字节长度
    [[nodiscard]] virtual auto recvFrameSize() const -> size_t = 0;

    /// 返回发送帧的固定字节长度
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t = 0;
};

}  // namespace Dss::Comm
