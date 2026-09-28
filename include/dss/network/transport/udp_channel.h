#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
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
    /** @brief 构造有界 UDP 通道，套接字在 bind 时创建。
     * @param maxPendingRequests 等待发送的条数上限，可设为零以拒绝排队。
     * @param maxPendingBytes 等待发送的载荷字节上限，不含当前执行的一条报文。
     */
    explicit UdpChannel(std::size_t maxPendingRequests = 64,
                        std::size_t maxPendingBytes = 1024 * 1024);
    /// 单报文载荷预算；实际可发送大小还取决于网络与操作系统。
    static constexpr std::size_t maxDatagramBytes = 65507;
    /// 发送队列的一致快照；计数在对象存续期间累计，重新 bind 不清零拒绝计数。
    struct SendQueueSnapshot {
        std::size_t pendingRequests;     ///< 尚未执行的发送请求数。
        std::size_t pendingBytes;        ///< 尚未执行的载荷字节数。
        std::uint64_t rejectedRequests;  ///< 超预算、未开启或正在关闭时的拒绝数。
        bool accepting;                  ///< 当前是否接受发送请求。
    };
    /// @brief 查询发送队列状态。 @return 在同一锁内取得的快照。
    [[nodiscard]] auto sendQueueSnapshot() const -> SendQueueSnapshot;
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

    /// 封闭发送入口并唤醒工作线程；已接受的请求逐条尝试发送后 join，不保证对端收到。
    /// 必须由外部线程调用，接收回调不得重入 bind/close；关闭需等待当前回调返回。
    void close();

    /**
     * @brief 向配置中的默认远程地址发送数据
     * @param data 待发送的字节数据
     * @return 实际 socket 写入字节数；未开启、关闭中、超预算或写入失败返回 -1。
     * @note 等待本条报文发送完成；回调内同通道发送直接执行，不进入等待队列。
     */
    auto send(std::span<const uint8_t> data) -> int64_t;

    /**
     * @brief 向指定主机和端口发送数据
     * @param data 待发送的字节数据
     * @param host 数值形式的目标 IP 地址（含可选 scope，最多 256 字符）
     * @param port 目标端口号
     * @return 实际 socket 写入字节数；未开启、关闭中、超预算或写入失败返回 -1。
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
     * @brief 每轮最多处理 64 条发送命令，避免接收与停止检查饥饿。
     * @param socket 当前 I/O 工作线程独占的已绑定套接字。
     */
    void processPendingSends(QUdpSocket& socket);
    /**
     * @brief 每轮最多读取 64 条数据报，在无锁状态调用回调；回调异常使通道关闭。
     * @param socket 当前 I/O 工作线程独占的已绑定套接字。
     * @param token 关闭令牌；停止后不再读取新数据报。
     */
    void onReadyRead(QUdpSocket& socket, std::stop_token token);
    /// 在已持有生命周期锁时停止工作线程。
    void closeLocked();
    /// 让尚未执行的发送请求以失败结果结束。
    void failPendingSends();

    std::jthread m_workerThread;                       ///< 独占 QUdpSocket 的 I/O 工作线程。
    mutable std::mutex m_lifecycleMutex;               ///< 串行化 bind/close。
    mutable std::mutex m_sendMutex;                    ///< 保护发送队列与默认远端配置。
    std::condition_variable m_sendWake;                ///< 发送与关闭唤醒；接收仍周期检查。
    const std::size_t m_maxPendingRequests;            ///< 队列条数上限。
    const std::size_t m_maxPendingBytes;               ///< 队列载荷上限。
    std::size_t m_pendingBytes = 0;                    ///< 当前等待载荷字节数。
    std::uint64_t m_rejectedRequests = 0;              ///< 累计拒绝次数。
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
