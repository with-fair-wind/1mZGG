#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>

#include "dss/network/i_network_channel.h"

class QUdpSocket;

namespace Dss::Network {

/// UDP 通道实现，基于 Qt QUdpSocket 完成绑定、发送与异步接收
class UdpChannel {
public:
    /// 构造 UDP 通道（套接字在 bind 时创建）
    UdpChannel();
    /// 析构时自动关闭套接字
    ~UdpChannel();

    UdpChannel(const UdpChannel&) = delete;
    UdpChannel& operator=(const UdpChannel&) = delete;

    /**
     * @brief 绑定本地 UDP 端点
     * @param config UDP 端点配置（本地 IP、端口及默认远程目标；端口 0 表示由系统分配）
     * @return 成功返回空值，失败返回错误描述
     */
    auto bind(const UdpEndpointConfig& config) -> std::expected<void, std::string>;

    /// 关闭套接字并释放资源
    void close();

    /**
     * @brief 向配置中的默认远程地址发送数据
     * @param data 待发送的字节数据
     * @return 成功发送的字节数，失败返回 -1
     */
    auto send(std::span<const uint8_t> data) -> int64_t;

    /**
     * @brief 向指定主机和端口发送数据
     * @param data 待发送的字节数据
     * @param host 目标主机地址
     * @param port 目标端口号
     * @return 成功发送的字节数，失败返回 -1
     */
    auto sendTo(std::span<const uint8_t> data, const std::string& host, uint16_t port) -> int64_t;

    /// 查询套接字是否已成功绑定
    [[nodiscard]] bool isBound() const;

    /**
     * @brief 设置数据报接收回调
     * @param cb 回调函数，参数为载荷数据、发送方 IP 与端口
     */
    void setReceiveCallback(
        std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb);

private:
    /// Qt 套接字可读信号的处理函数
    void onReadyRead();

    std::unique_ptr<QUdpSocket> m_socket;  ///< Qt UDP 套接字对象
    UdpEndpointConfig m_config{};          ///< 当前端点配置
    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)>
        m_recvCallback;  ///< 数据报接收回调
    std::mutex m_mutex;  ///< 保护接收回调的互斥锁
};

}  // namespace Dss::Network
