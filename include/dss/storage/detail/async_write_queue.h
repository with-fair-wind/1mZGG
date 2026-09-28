#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <expected>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "dss/core/diagnostics/resource_snapshot.h"

namespace Dss::Storage {

/**
 * @brief 后台异步写入队列。
 *
 * 该类封装存储后端共享的 jthread 生命周期、背压队列、停止排空和写入计数逻辑。
 * 具体写入格式与错误事件仍由调用方通过回调提供。
 *
 * @tparam Request 队列中的写入请求类型。
 */
template <typename Request>
class AsyncWriteQueue {
public:
    using Writer =
        std::function<std::expected<void, std::string>(const Request&)>;  ///< 单请求写入函数。
    using FailureHandler =
        std::function<void(const Request&, const std::string&)>;  ///< 写入失败通知函数。

    /**
     * @brief 构造异步写入队列。
     * @param maxPendingRequests 队列允许的最大待处理请求数。
     * @param maxPendingBytes 等待与正在写入的载荷总字节预算。
     */
    explicit AsyncWriteQueue(
        std::size_t maxPendingRequests = 1024,
        std::size_t maxPendingBytes = (std::numeric_limits<std::size_t>::max)())
        : m_maxPendingRequests(maxPendingRequests), m_maxPendingBytes(maxPendingBytes) {}

    /** @brief 停止并排空队列后销毁。 */
    ~AsyncWriteQueue() {
        stop();
    }

    AsyncWriteQueue(const AsyncWriteQueue&) = delete;
    auto operator=(const AsyncWriteQueue&) -> AsyncWriteQueue& = delete;
    AsyncWriteQueue(AsyncWriteQueue&&) = delete;
    auto operator=(AsyncWriteQueue&&) -> AsyncWriteQueue& = delete;

    /**
     * @brief 启动后台写入线程。
     * @param writer 处理单个写入请求的函数。
     * @param failureHandler 写入失败后的通知函数。
     * @return 成功时返回空；写入函数无效时返回错误描述。
     */
    auto start(Writer writer, FailureHandler failureHandler = {})
        -> std::expected<void, std::string> {
        std::lock_guard lifecycleLock(m_lifecycleMutex);
        if (!writer) {
            return std::unexpected("async write queue writer is not configured");
        }
        if (m_worker.joinable()) {
            if (isRunning()) {
                return {};
            }
            return std::unexpected("async write queue must finish stop before restarting");
        }

        m_writer = std::move(writer);
        m_failureHandler = std::move(failureHandler);
        {
            std::lock_guard queueLock(m_queueMutex);
            m_accepting = true;
        }
        m_running.store(true);
        m_worker = std::jthread([this](std::stop_token token) {
            try {
                workerLoop(token);
            } catch (...) {
                // Protect the boundary even if moving a request or allocating diagnostics fails.
                m_failedWrites.fetch_add(1);
                std::lock_guard lock(m_queueMutex);
                m_queue.clear();
                m_pendingBytes = 0;
                m_resources.activeItems = 0;
                m_resources.activeBytes = 0;
                m_accepting = false;
            }
            m_running.store(false);
        });
        return {};
    }

    /** @brief 关闭准入并唤醒 worker，继续排空已接受请求；不等待磁盘写入。 */
    void requestStop() {
        {
            std::lock_guard queueLock(m_queueMutex);
            m_accepting = false;
            m_running.store(false);
        }
        m_queueCv.notify_all();
    }

    /** @brief 请求后台线程停止，并等待队列排空后退出。 */
    void stop() {
        const auto started = std::chrono::steady_clock::now();
        std::lock_guard lifecycleLock(m_lifecycleMutex);
        requestStop();
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_queueCv.notify_all();
            m_worker.join();
        }
        std::lock_guard queueLock(m_queueMutex);
        m_resources.lastStopMicroseconds = Dss::Core::elapsedMicroseconds(started);
    }

    /** @brief 查询队列是否接受新请求。 @return start 成功且尚未开始 stop 时返回 true。 */
    [[nodiscard]] auto isRunning() const -> bool {
        return m_running.load();
    }

    /**
     * @brief 将请求加入写入队列。
     * @param request 待写入请求。
     * @param bytes 本请求保留的载荷字节数，用于预算与资源计数。
     * @return 入队成功时返回空；线程未运行或队列已满时返回错误描述。
     */
    auto enqueue(Request request, std::size_t bytes = 0) -> std::expected<void, std::string> {
        {
            std::lock_guard lock(m_queueMutex);
            if (!m_accepting) {
                return std::unexpected("async write queue is not running");
            }
            if (m_queue.size() >= m_maxPendingRequests) {
                m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
                return std::unexpected("async write queue is full");
            }
            if (bytes > m_maxPendingBytes - m_pendingBytes) {
                m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
                return std::unexpected("async write queue byte budget exceeded");
            }
            m_queue.push_back(Entry{std::move(request), bytes});
            m_pendingBytes += bytes;
            m_resources.peakQueuedBytes =
                (std::max)(m_resources.peakQueuedBytes,
                           static_cast<std::uint64_t>(m_pendingBytes) - m_resources.activeBytes);
        }
        m_queueCv.notify_one();
        return {};
    }

    /** @brief 获取因队列满而拒绝的请求数量。 @return 丢弃请求计数。 */
    [[nodiscard]] auto droppedRequests() const -> std::uint64_t {
        return m_droppedRequests.load();
    }

    /** @brief 获取成功写入的请求数量。 @return 成功写入计数。 */
    [[nodiscard]] auto successfulWrites() const -> std::uint64_t {
        return m_successfulWrites.load();
    }

    /** @brief 获取写入失败的请求数量。 @return 失败写入计数。 */
    [[nodiscard]] auto failedWrites() const -> std::uint64_t {
        return m_failedWrites.load();
    }

    /** @brief 待写和正在写入的请求共持有的载荷字节数。 */
    /// @return 等待及正在写入请求的载荷总字节数。
    [[nodiscard]] auto pendingBytes() const -> std::size_t {
        std::lock_guard lock(m_queueMutex);
        return m_pendingBytes;
    }

    /** @brief 获取本组件的资源采样。 @return 队列锁内采样的等待/活动载荷、完成数与耗时。 */
    [[nodiscard]] auto resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
        std::lock_guard lock(m_queueMutex);
        auto result = m_resources;
        result.queuedItems = m_queue.size();
        result.queuedBytes = m_pendingBytes - result.activeBytes;
        return result;
    }

private:
    /// 保留请求及其预算计量，等待和写入阶段共用该条目。
    struct Entry {
        Request request;    ///< 拥有的写入请求。
        std::size_t bytes;  ///< 本请求保留的载荷字节数。
    };
    /**
     * @brief 持续消费队列并调用写入回调。
     * @param token 停止令牌；收到停止请求后仍会排空队列。
     */
    void workerLoop(std::stop_token token) {
        while (true) {
            std::optional<Entry> entry;
            {
                std::unique_lock lock(m_queueMutex);
                m_queueCv.wait(lock, token, [this, &token] {
                    return !m_queue.empty() || !m_accepting || token.stop_requested();
                });
                if (m_queue.empty()) {
                    break;
                }
                entry = std::move(m_queue.front());
                m_queue.pop_front();
                m_resources.activeItems = 1;
                m_resources.activeBytes = entry->bytes;
            }

            const auto started = std::chrono::steady_clock::now();
            std::expected<void, std::string> result;
            try {
                result = m_writer(entry->request);
            } catch (const std::exception& error) {
                result = std::unexpected(error.what());
            } catch (...) {
                result = std::unexpected("unknown writer exception");
            }
            if (result) {
                m_successfulWrites.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_failedWrites.fetch_add(1, std::memory_order_relaxed);
                if (m_failureHandler) {
                    try {
                        m_failureHandler(entry->request, result.error());
                    } catch (...) {
                        // Reporting failure must not terminate the worker or prevent draining.
                    }
                }
            }
            const auto bytes = entry->bytes;
            entry.reset();
            {
                std::lock_guard lock(m_queueMutex);
                m_pendingBytes -= bytes;
                m_resources.activeItems = 0;
                m_resources.activeBytes = 0;
                ++m_resources.completedItems;
                m_resources.lastWorkMicroseconds = Dss::Core::elapsedMicroseconds(started);
                m_resources.maxWorkMicroseconds =
                    (std::max)(m_resources.maxWorkMicroseconds, m_resources.lastWorkMicroseconds);
            }
        }
    }

    Dss::Core::ResourceSnapshot m_resources;           ///< 队列锁保护。
    std::atomic<bool> m_running{false};                ///< 后台线程运行状态。
    std::size_t m_maxPendingRequests = 1024;           ///< 写入队列容量上限。
    const std::size_t m_maxPendingBytes;               ///< 包括在写请求的载荷预算。
    std::size_t m_pendingBytes = 0;                    ///< 由队列锁保护。
    std::atomic<std::uint64_t> m_droppedRequests{0};   ///< 被背压拒绝的请求数量。
    std::atomic<std::uint64_t> m_successfulWrites{0};  ///< 成功写入数量。
    std::atomic<std::uint64_t> m_failedWrites{0};      ///< 写入失败数量。
    Writer m_writer;                                   ///< 单个请求写入函数。
    FailureHandler m_failureHandler;                   ///< 写入失败通知函数。
    std::jthread m_worker;                             ///< 后台写入工作线程。
    std::mutex m_lifecycleMutex;                       ///< 串行化启动与停止操作。
    mutable std::mutex m_queueMutex;                   ///< 保护写入队列的互斥锁。
    std::condition_variable_any m_queueCv;             ///< 写入队列条件变量。
    std::deque<Entry> m_queue;                         ///< 待写入请求队列。
    bool m_accepting = false;                          ///< 是否接受新的写入请求。
};

}  // namespace Dss::Storage
