#pragma once

#include <atomic>
#include <expected>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/network/diagnostic_protocol.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

/// 错误诊断服务，周期性通过 UDP 发送各子系统状态的 JSON 报文
class ErrorDiagnostics : public INetworkChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造错误诊断服务
     * @param bus 事件总线引用（预留扩展）
     */
    explicit ErrorDiagnostics(MessageBus& bus);

    /// 析构时自动关闭通道并停止工作线程
    ~ErrorDiagnostics() override;

    /**
     * @brief 绑定 UDP 端点并启动周期性发送线程
     * @param config UDP 端点配置
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> override;

    /// 停止工作线程并关闭 UDP 通道
    void close() override;

    /** @brief 查询诊断通道是否已绑定。 @return UDP 通道可发送时返回 true。 */
    [[nodiscard]] bool isOpen() const override;

    /** @brief 获取诊断服务状态。 @return 已绑定时为 Running，否则为 Init。 */
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /**
     * @brief 更新诊断状态快照
     * @param status 新的诊断状态
     */
    void setStatus(const DiagnosticStatus& status);

    /**
     * @brief 获取线程安全的诊断状态快照。
     * @return 当前子系统状态的副本。
     */
    [[nodiscard]] auto statusSnapshot() const -> DiagnosticStatus;

private:
    /**
     * @brief 工作线程主循环，每秒发送一次诊断 JSON
     * @param token 停止令牌，用于优雅退出
     */
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;                                      ///< 事件总线引用
    UdpChannel m_channel;                                   ///< 诊断报文 UDP 通道
    std::jthread m_workerThread;                            ///< 周期性发送工作线程
    mutable std::mutex m_statusMutex;                       ///< 保护诊断状态的互斥锁
    DiagnosticStatus m_status{};                            ///< 最近一次设置的诊断状态
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接
};

}  // namespace Dss::Network
