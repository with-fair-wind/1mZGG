#pragma once

#include <expected>
#include <span>

#include "dss/core/event_bus.h"
#include "dss/network/data_exchange_protocol.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

/// 数据交换服务，通过双 UDP 通道分别发送 GXTC 与 GDCL 协议报文
class DataExchange {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造数据交换服务
     * @param bus 事件总线引用（预留扩展）
     */
    explicit DataExchange(MessageBus& bus);

    /**
     * @brief 打开 GXTC 与 GDCL 两个 UDP 通道
     * @param gxtcConfig GXTC 通道端点配置
     * @param gdclConfig GDCL 通道端点配置
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const UdpEndpointConfig& gxtcConfig, const UdpEndpointConfig& gdclConfig)
        -> std::expected<void, std::string>;

    /// 关闭所有 UDP 通道
    void close();

    /// 查询两个通道是否均已绑定
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief 发送原始 GXTC 报文
     * @param data 已编码的字节数据
     */
    void sendGxtc(std::span<const uint8_t> data);

    /**
     * @brief 编码并发送 GXTC 报文
     * @param metadata 报文头部元数据
     * @param targets 目标列表
     */
    void sendGxtc(const GxtcMetadata& metadata, std::span<const GxtcTarget> targets);

    /**
     * @brief 发送原始 GDCL 报文
     * @param data 已编码的字节数据
     */
    void sendGdcl(std::span<const uint8_t> data);

    /**
     * @brief 编码并发送 GDCL 报文
     * @param measurement 测量结果
     */
    void sendGdcl(const GdclMeasurement& measurement);

private:
    [[maybe_unused]] MessageBus& m_bus;  ///< 事件总线引用（预留扩展）
    UdpChannel m_gxtcChannel;            ///< GXTC 协议 UDP 通道
    UdpChannel m_gdclChannel;            ///< GDCL 协议 UDP 通道
};

}  // namespace Dss::Network
