#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <future>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "dss/network/transport/i_network_channel.h"

class QUdpSocket;

namespace Dss::Network {

/// UDP 通道实现；使用 std::jthread 串行执行 QUdpSocket 的绑定、收发与销毁
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

    /** @brief 查询套接字是否已绑定。 @return 本地端点绑定成功时返回 true。 */
    [[nodiscard]] bool isBound() const;

    /**
     * @brief 获取实际绑定的本地端口。
     * @return 绑定成功后的端口；未绑定时返回 0。使用配置端口 0 时可获取系统分配端口。
     */
    [[nodiscard]] auto localPort() const -> uint16_t;

    /**
     * @brief 设置数据报接收回调
     * @param cb 回调函数，参数为载荷数据、发送方 IP 与端口
     */
    void setReceiveCallback(
        std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb);

private:
    /// 跨线程发送命令及其同步完成通知。
    struct SendRequest {
        std::vector<uint8_t> data;     ///< 独立拥有的待发送字节。
        std::string host;              ///< 目标主机。
        uint16_t port = 0;             ///< 目标端口。
        std::promise<int64_t> result;  ///< 实际发送字节数。
    };

    /**
     * @brief I/O 工作循环，在单一线程内管理 QUdpSocket 完整生命周期。
     * @param token 停止令牌。
     * @param config 线程启动时的端点配置快照。
     * @param initPromise 绑定结果回传通道。
     */
    void workerLoop(std::stop_token token, UdpEndpointConfig config,
                    std::promise<std::expected<void, std::string>> initPromise);

    /**
     * @brief 处理已排队的全部发送命令。
     * @param socket 当前 I/O 工作线程独占的已绑定套接字。
     */
    void processPendingSends(QUdpSocket& socket);
    /**
     * @brief 读取全部待处理数据报并在无锁状态下调用接收回调。
     * @param socket 当前 I/O 工作线程独占的已绑定套接字。
     */
    void onReadyRead(QUdpSocket& socket);
    /// 在已持有生命周期锁时停止工作线程。
    void closeLocked();
    /// 让尚未执行的发送请求以失败结果结束。
    void failPendingSends();

    std::jthread m_workerThread;                       ///< 独占 QUdpSocket 的 I/O 工作线程。
    mutable std::mutex m_lifecycleMutex;               ///< 串行化 bind/close。
    std::mutex m_sendMutex;                            ///< 保护发送队列与默认远端配置。
    std::deque<SendRequest> m_sendQueue;               ///< 待执行发送命令。
    std::string m_remoteIp;                            ///< 默认远端 IP。
    uint16_t m_remotePort = 0;                         ///< 默认远端端口。
    bool m_acceptingSends = false;                     ///< 是否接受新的发送命令。
    std::atomic<bool> m_bound{false};                  ///< 套接字绑定状态。
    std::atomic<uint16_t> m_localPort{0};              ///< 实际绑定的本地端口。
    std::atomic<QUdpSocket*> m_workerSocket{nullptr};  ///< 仅供 I/O 线程重入发送的观察指针。

    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)>
        m_recvCallback;          ///< 数据报接收回调
    std::mutex m_callbackMutex;  ///< 保护接收回调的互斥锁
};

}  // namespace Dss::Network
