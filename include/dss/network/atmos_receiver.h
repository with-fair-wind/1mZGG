#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include "dss/core/event_bus.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

/// 气象数据 UDP 接收服务，解码报文并通过事件总线发布大气采样事件
class AtmosReceiver : public INetworkChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造气象接收服务
     * @param bus 事件总线引用，用于发布 AtmosphereDataEvent
     */
    explicit AtmosReceiver(MessageBus& bus);

    /**
     * @brief 绑定 UDP 端点并注册接收回调
     * @param config UDP 端点配置
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> override;

    /// 关闭 UDP 通道
    void close() override;

    /** @brief 查询接收通道是否已绑定。 @return UDP 通道可接收数据时返回 true。 */
    [[nodiscard]] bool isOpen() const override;

    /** @brief 获取接收服务状态。 @return 已绑定时为 Running，否则为 Init。 */
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

private:
    /**
     * @brief 处理收到的气象数据报
     * @param data 原始载荷字节
     * @param sender 发送方 IP 地址
     * @param port 发送方端口号
     */
    void onData(std::span<const uint8_t> data, const std::string& sender, uint16_t port);

    MessageBus& m_bus;     ///< 事件总线引用
    UdpChannel m_channel;  ///< 气象数据 UDP 通道
};

}  // namespace Dss::Network
