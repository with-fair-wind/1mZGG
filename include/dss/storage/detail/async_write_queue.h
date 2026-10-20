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
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "dss/core/diagnostics/emergency_report.h"
#include "dss/core/diagnostics/resource_snapshot.h"

namespace Dss::Storage {

/// @brief 可等待存储准入的终止原因；取消/停止与真实写入失败分开处理。
enum class WriteAdmissionError : std::uint8_t {
    Stopped,           ///< 队列已关闭准入。
    Cancelled,         ///< 调用方取消等待。
    StaleSession,      ///< 请求属于旧会话或旧 worker。
    CapacityExceeded,  ///< 请求本身超过预算或队列容量为零。
    WriterFailed,      ///< 本次启动已有写入失败，无损准入停止。
    NotReady,          ///< 后端尚未初始化。
    NoSession,         ///< 未配置图像会话。
    InvalidRequest,    ///< 请求载荷无效。
};

/// @brief 获取可等待准入错误的稳定诊断文本。
/// @param error 准入错误类别。
/// @return 静态存储期文本；磁盘失败的具体原因另由 StorageWriteErrorEvent 报告。
[[nodiscard]] inline auto writeAdmissionErrorMessage(WriteAdmissionError error)
    -> std::string_view {
    switch (error) {
        case WriteAdmissionError::Stopped:
            return "storage admission stopped";
        case WriteAdmissionError::Cancelled:
            return "storage admission cancelled";
        case WriteAdmissionError::StaleSession:
            return "stale storage admission session";
        case WriteAdmissionError::CapacityExceeded:
            return "storage request exceeds queue capacity";
        case WriteAdmissionError::WriterFailed:
            return "storage writer failed; see diagnostics";
        case WriteAdmissionError::NotReady:
            return "storage backend is not initialized";
        case WriteAdmissionError::NoSession:
            return "image storage session is not configured";
        case WriteAdmissionError::InvalidRequest:
            return "storage request payload is invalid";
    }
    return "unknown storage admission error";
}

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
    explicit AsyncWriteQueue(std::size_t maxPendingRequests = 1024,
                             std::size_t maxPendingBytes = std::numeric_limits<std::size_t>::max())
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
        std::scoped_lock lifecycleLock(m_lifecycleMutex);
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
            std::scoped_lock queueLock(m_queueMutex);
            m_accepting = true;
            m_writeFailed = false;
            ++m_generation;
        }
        m_running.store(true);
        m_worker = std::jthread([this](std::stop_token token) {
            try {
                workerLoop(token);
            } catch (...) {
                // Protect the boundary even if moving a request or allocating diagnostics fails.
                m_failedWrites.fetch_add(1);
                {
                    std::scoped_lock lock(m_queueMutex);
                    m_queue.clear();
                    m_pendingBytes = 0;
                    m_resources.activeItems = 0;
                    m_resources.activeBytes = 0;
                    m_accepting = false;
                    m_writeFailed = true;
                }
                m_spaceCv.notify_all();
            }
            m_running.store(false);
        });
        m_spaceCv.notify_all();
        return {};
    }

    /** @brief 关闭准入并唤醒 worker，继续排空已接受请求；不等待磁盘写入。 */
    void requestStop() {
        {
            std::scoped_lock queueLock(m_queueMutex);
            m_accepting = false;
            m_running.store(false);
        }
        m_queueCv.notify_all();
        m_spaceCv.notify_all();
    }

    /** @brief 请求后台线程停止，并等待队列排空后退出。 */
    void stop() {
        const auto started = std::chrono::steady_clock::now();
        std::scoped_lock lifecycleLock(m_lifecycleMutex);
        requestStop();
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_queueCv.notify_all();
            m_worker.join();
        }
        std::scoped_lock queueLock(m_queueMutex);
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
            std::scoped_lock lock(m_queueMutex);
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
            pushLocked(std::move(request), bytes);
        }
        m_queueCv.notify_one();
        return {};
    }

    /// @brief 获取当前 worker 的准入代次；调用方应在后端会话锁内捕获。
    /// @return 每次 start 增加的代次，用于防止等待请求穿越停止/重启。
    [[nodiscard]] auto generation() const -> std::uint64_t {
        std::scoped_lock lock(m_queueMutex);
        return m_generation;
    }

    /**
     * @brief 等待容量后入队；不持有调用方的会话锁，不能从本队列 writer 回调调用。
     * @param request 等待期间由调用线程持有的请求。
     * @param bytes 载荷预算字节数，包括在写请求。
     * @param token 取消等待的令牌。
     * @param generation 后端校验会话时捕获的 worker 代次。
     * @return 入队成功或类型化终止原因；取消、旧代次及停止不累计容量丢弃。
     * @note 调用方须限制并发生产者数量；等待者载荷不属于已接受队列预算。
     * 本轮发生写入失败后拒绝新的无损请求，但继续排空已接受项。
     */
    auto enqueueBlocking(Request request, std::size_t bytes, std::stop_token token,
                         std::uint64_t generation) -> std::expected<void, WriteAdmissionError> {
        std::unique_lock lock(m_queueMutex);
        if (const auto error = admissionErrorLocked(token, generation)) {
            return std::unexpected(*error);
        }
        if (m_maxPendingRequests == 0 || bytes > m_maxPendingBytes) {
            m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
            return std::unexpected(WriteAdmissionError::CapacityExceeded);
        }
        if (!hasCapacityLocked(bytes)) {
            const WaitingRequest waiting(m_resources.waitingItems);
            m_resources.peakWaitingItems =
                (std::max)(m_resources.peakWaitingItems, m_resources.waitingItems);
            m_spaceCv.wait(lock, token, [this, token, generation, bytes] {
                return admissionErrorLocked(token, generation).has_value() ||
                       hasCapacityLocked(bytes);
            });
        }
        if (const auto error = admissionErrorLocked(token, generation)) {
            return std::unexpected(*error);
        }
        pushLocked(std::move(request), bytes);
        lock.unlock();
        m_queueCv.notify_one();
        return {};
    }

    /// @brief 不关闭准入，等待所有已接受项完成；本轮写入失败会返回错误。
    /// @pre 由外部协调线程调用，不能从 writer 或 failureHandler 等待自身。
    /// @param token 取消等待的令牌。
    /// @return 排空成功，或取消、停止、重启、写入失败；不累计容量丢弃。
    auto waitUntilIdle(std::stop_token token) -> std::expected<void, WriteAdmissionError> {
        std::unique_lock lock(m_queueMutex);
        const auto generation = m_generation;
        m_spaceCv.wait(lock, token, [this, token, generation] {
            return admissionErrorLocked(token, generation).has_value() ||
                   (m_queue.empty() && m_resources.activeItems == 0);
        });
        if (const auto error = admissionErrorLocked(token, generation)) {
            return std::unexpected(*error);
        }
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
        std::scoped_lock lock(m_queueMutex);
        return m_pendingBytes;
    }

    /** @brief 获取本组件的资源采样。 @return 队列锁内采样的等待/活动载荷、完成数与耗时。 */
    [[nodiscard]] auto resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
        std::scoped_lock lock(m_queueMutex);
        auto result = m_resources;
        result.queuedItems = m_queue.size();
        result.queuedBytes = m_pendingBytes - result.activeBytes;
        return result;
    }

private:
    /// @brief 队列锁内维护可观测等待数；条件变量返回后仍持有同一队列锁。
    struct WaitingRequest {
        /// @brief 记录一个正在等待准入的调用者。
        /// @param counter 由队列互斥锁保护的等待者计数。
        explicit WaitingRequest(std::uint64_t& counter) : count(counter) {
            ++count;
        }
        WaitingRequest(const WaitingRequest&) = delete;
        auto operator=(const WaitingRequest&) -> WaitingRequest& = delete;
        WaitingRequest(WaitingRequest&&) = delete;
        auto operator=(WaitingRequest&&) -> WaitingRequest& = delete;
        /// @brief 结束等待并恢复计数，调用期间仍持有队列锁。
        ~WaitingRequest() {
            --count;
        }
        std::uint64_t& count;  ///< 队列锁保护的等待数。
    };

    /// @brief 在队列锁内检查取消、代次、停止及写入失败，先于容量判断。
    /// @param token 调用方取消令牌。
    /// @param generation 捕获的 worker 代次。
    /// @return 无错误时为空。
    auto admissionErrorLocked(std::stop_token token, std::uint64_t generation) const
        -> std::optional<WriteAdmissionError> {
        if (token.stop_requested()) {
            return WriteAdmissionError::Cancelled;
        }
        if (generation != m_generation) {
            return WriteAdmissionError::StaleSession;
        }
        if (m_writeFailed) {
            return WriteAdmissionError::WriterFailed;
        }
        if (!m_accepting) {
            return WriteAdmissionError::Stopped;
        }
        return std::nullopt;
    }

    /// @brief 检查剩余请求数与字节预算；调用方持有队列锁。
    /// @param bytes 请求载荷字节数。
    /// @return 两项预算均可容纳请求时为 true。
    auto hasCapacityLocked(std::size_t bytes) const -> bool {
        return m_queue.size() < m_maxPendingRequests && bytes <= m_maxPendingBytes - m_pendingBytes;
    }

    /// @brief 在队列锁内记录一项已接受请求，调用方已校验预算。
    /// @param request 移交给队列的请求。
    /// @param bytes 载荷字节数。
    void pushLocked(Request&& request, std::size_t bytes) {
        m_queue.push_back(Entry{std::move(request), bytes});
        m_pendingBytes += bytes;
        m_resources.peakQueuedBytes =
            (std::max)(m_resources.peakQueuedBytes,
                       static_cast<std::uint64_t>(m_pendingBytes) - m_resources.activeBytes);
    }

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

            m_spaceCv.notify_all();
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
                {
                    std::scoped_lock lock(m_queueMutex);
                    m_writeFailed = true;
                }
                m_spaceCv.notify_all();
                if (m_failureHandler) {
                    try {
                        m_failureHandler(entry->request, result.error());
                    } catch (...) {
                        Dss::Core::reportEmergencyFailure("DSS: storage error callback failed\n");
                    }
                }
            }
            const auto bytes = entry->bytes;
            entry.reset();
            {
                std::scoped_lock lock(m_queueMutex);
                m_pendingBytes -= bytes;
                m_resources.activeItems = 0;
                m_resources.activeBytes = 0;
                ++m_resources.completedItems;
                m_resources.lastWorkMicroseconds = Dss::Core::elapsedMicroseconds(started);
                m_resources.maxWorkMicroseconds =
                    (std::max)(m_resources.maxWorkMicroseconds, m_resources.lastWorkMicroseconds);
            }
            m_spaceCv.notify_all();
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
    std::condition_variable_any m_spaceCv;             ///< 容量与完成等待，独立于 worker 唤醒。
    std::uint64_t m_generation = 0;                    ///< 每次 start 增加，队列锁保护。
    bool m_writeFailed = false;                        ///< 本次启动已有写入失败，队列锁保护。
    std::condition_variable_any m_queueCv;             ///< 写入队列条件变量。
    std::deque<Entry> m_queue;                         ///< 待写入请求队列。
    bool m_accepting = false;                          ///< 是否接受新的写入请求。
};

}  // namespace Dss::Storage
