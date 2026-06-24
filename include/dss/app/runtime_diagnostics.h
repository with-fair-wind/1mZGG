#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

#include "dss/core/event_bus.h"

namespace Dss::App {

/// @brief 运行期处理、存储与通信计数器的只读快照。
struct RuntimeDiagnosticsSnapshot {
    std::uint64_t processingDroppedFrames = 0;  ///< 处理管线丢弃的帧数
    std::uint64_t imageSuccessfulWrites = 0;    ///< 图像成功写入次数
    std::uint64_t imageFailedWrites = 0;        ///< 图像写入失败次数
    std::uint64_t imageDroppedRequests = 0;     ///< 图像存储队列丢弃的请求数
    std::uint64_t trackSuccessfulWrites = 0;    ///< 跟踪数据成功写入次数
    std::uint64_t trackFailedWrites = 0;        ///< 跟踪数据写入失败次数
    std::uint64_t trackDroppedRequests = 0;     ///< 跟踪存储队列丢弃的请求数
    std::uint64_t networkErrors = 0;            ///< 网络传输错误事件数
    std::uint64_t serialErrors = 0;             ///< 串口帧或解码错误事件数
    std::uint64_t storageErrors = 0;            ///< 存储错误事件数
};

/// @brief 从各运行服务读取诊断计数器的回调集合。
struct RuntimeDiagnosticsSources {
    std::function<std::uint64_t()> processingDroppedFrames;  ///< 读取处理丢帧数
    std::function<std::uint64_t()> imageSuccessfulWrites;    ///< 读取图像成功写入数
    std::function<std::uint64_t()> imageFailedWrites;        ///< 读取图像写入失败数
    std::function<std::uint64_t()> imageDroppedRequests;     ///< 读取图像丢弃请求数
    std::function<std::uint64_t()> trackSuccessfulWrites;    ///< 读取跟踪成功写入数
    std::function<std::uint64_t()> trackFailedWrites;        ///< 读取跟踪写入失败数
    std::function<std::uint64_t()> trackDroppedRequests;     ///< 读取跟踪丢弃请求数
};

/**
 * @brief 汇总处理、存储及通信子系统的运行期诊断数据。
 *
 * 实例通过事件总线累积错误事件，并通过回调按需读取各服务内部计数器。
 */
class RuntimeDiagnostics {
public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 跨线程消息总线类型

    /**
     * @brief 创建诊断汇总器并订阅错误事件。
     * @param bus 应用消息总线。
     * @param sources 各服务诊断计数器的读取回调。
     */
    RuntimeDiagnostics(MessageBus& bus, RuntimeDiagnosticsSources sources);

    /**
     * @brief 读取当前诊断数据。
     * @return 各计数器在调用时刻的快照。
     */
    [[nodiscard]] auto snapshot() const -> RuntimeDiagnosticsSnapshot;

private:
    RuntimeDiagnosticsSources m_sources;                    ///< 服务计数器读取回调
    std::atomic<std::uint64_t> m_networkErrors{0};          ///< 累积网络错误数
    std::atomic<std::uint64_t> m_serialErrors{0};           ///< 累积串口错误数
    std::atomic<std::uint64_t> m_storageErrors{0};          ///< 累积存储错误数
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 错误事件订阅连接
};

}  // namespace Dss::App
