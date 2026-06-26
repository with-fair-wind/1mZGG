#pragma once

#include <expected>
#include <string>

#include "dss/core/config_types.h"
#include "dss/core/constants.h"

namespace Dss::Network {

using UdpEndpointConfig = Dss::Core::UdpEndpointConfig;  ///< UDP 端点配置类型别名

/// 网络通道抽象接口，定义 UDP 端点的打开/关闭及状态查询能力
class INetworkChannel {
public:
    virtual ~INetworkChannel() = default;

    /**
     * @brief 打开网络通道
     * @param config UDP 端点配置（本地/远程 IP 与端口）
     * @return 成功返回空值，失败返回错误描述
     */
    virtual auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> = 0;

    /// 关闭通道并释放资源
    virtual void close() = 0;

    /** @brief 查询通道是否已打开。 @return 通道可收发数据时返回 true。 */
    [[nodiscard]] virtual bool isOpen() const = 0;

    /** @brief 获取当前通道状态。 @return 生命周期状态。 */
    [[nodiscard]] virtual auto status() const -> Dss::Core::Status = 0;
};

}  // namespace Dss::Network
