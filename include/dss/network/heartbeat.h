#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <thread>

#include "dss/core/event_bus.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

/// 心跳服务，周期性通过 UDP 发送固定格式心跳帧以维持链路存活
class Heartbeat : public INetworkChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造心跳服务
     * @param bus 事件总线引用（预留扩展）
     */
    explicit Heartbeat(MessageBus& bus);

    /// 析构时自动关闭通道并停止工作线程
    ~Heartbeat() override;

    /**
     * @brief 绑定 UDP 端点并启动周期性心跳线程
     * @param config UDP 端点配置
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> override;

    /// 停止工作线程并关闭 UDP 通道
    void close() override;

    /** @brief 查询心跳通道是否已绑定。 @return UDP 通道可发送时返回 true。 */
    [[nodiscard]] bool isOpen() const override;

    /** @brief 获取心跳服务状态。 @return 已绑定时为 Running，否则为 Init。 */
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /// 立即发送一帧关闭守护心跳（用于进程退出前通知对端）
    void sendCloseGuard();

    /**
     * @brief 构建标准心跳帧。
     * @return 包含帧头、载荷与帧尾的 10 字节数组。
     */
    [[nodiscard]] static auto buildFrame() -> std::array<uint8_t, 10>;

    /**
     * @brief 构建关闭守护心跳帧。
     * @return 在标准帧基础上设置关闭标志的 10 字节数组。
     */
    [[nodiscard]] static auto buildCloseGuardFrame() -> std::array<uint8_t, 10>;

private:
    /**
     * @brief 工作线程主循环，每 100ms 发送一次心跳帧
     * @param token 停止令牌，用于优雅退出
     */
    void workerLoop(std::stop_token token);

    [[maybe_unused]] MessageBus& m_bus;  ///< 事件总线引用（预留扩展）
    UdpChannel m_channel;                ///< 心跳 UDP 通道
    std::jthread m_workerThread;         ///< 周期性发送工作线程
};

}  // namespace Dss::Network
