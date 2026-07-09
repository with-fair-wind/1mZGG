#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

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
    using Writer = std::function<std::expected<void, std::string>(const Request&)>;
    using FailureHandler = std::function<void(const Request&, const std::string&)>;

    /**
     * @brief 构造异步写入队列。
     * @param maxPendingRequests 队列允许的最大待处理请求数。
     */
    explicit AsyncWriteQueue(std::size_t maxPendingRequests = 1024)
        : m_maxPendingRequests(maxPendingRequests) {}

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
        if (!writer) {
            return std::unexpected("async write queue writer is not configured");
        }
        if (m_worker.joinable()) {
            return {};
        }

        m_writer = std::move(writer);
        m_failureHandler = std::move(failureHandler);
        m_running.store(true);
        m_worker = std::jthread([this](std::stop_token token) { workerLoop(token); });
        return {};
    }

    /** @brief 请求后台线程停止，并等待队列排空后退出。 */
    void stop() {
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_queueCv.notify_all();
            m_worker.join();
        }
        m_running.store(false);
    }

    /** @brief 查询后台线程是否运行。 @return 线程运行时返回 true。 */
    [[nodiscard]] auto isRunning() const -> bool {
        return m_running.load();
    }

    /**
     * @brief 将请求加入写入队列。
     * @param request 待写入请求。
     * @return 入队成功时返回空；线程未运行或队列已满时返回错误描述。
     */
    auto enqueue(Request request) -> std::expected<void, std::string> {
        if (!m_running.load()) {
            return std::unexpected("async write queue is not running");
        }

        {
            std::lock_guard lock(m_queueMutex);
            if (m_queue.size() >= m_maxPendingRequests) {
                m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
                return std::unexpected("async write queue is full");
            }
            m_queue.push_back(std::move(request));
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

private:
    /**
     * @brief 持续消费队列并调用写入回调。
     * @param token 停止令牌；收到停止请求后仍会排空队列。
     */
    void workerLoop(std::stop_token token) {
        while (true) {
            std::optional<Request> request;
            {
                std::unique_lock lock(m_queueMutex);
                m_queueCv.wait(lock, token, [this, &token] {
                    return !m_queue.empty() || token.stop_requested();
                });
                if (m_queue.empty()) {
                    break;
                }
                request = std::move(m_queue.front());
                m_queue.pop_front();
            }

            const auto result = m_writer(*request);
            if (result) {
                m_successfulWrites.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_failedWrites.fetch_add(1, std::memory_order_relaxed);
                if (m_failureHandler) {
                    m_failureHandler(*request, result.error());
                }
            }
        }
    }

    std::atomic<bool> m_running{false};                ///< 后台线程运行状态。
    std::size_t m_maxPendingRequests = 1024;           ///< 写入队列容量上限。
    std::atomic<std::uint64_t> m_droppedRequests{0};   ///< 被背压拒绝的请求数量。
    std::atomic<std::uint64_t> m_successfulWrites{0};  ///< 成功写入数量。
    std::atomic<std::uint64_t> m_failedWrites{0};      ///< 写入失败数量。
    Writer m_writer;                                   ///< 单个请求写入函数。
    FailureHandler m_failureHandler;                   ///< 写入失败通知函数。
    std::jthread m_worker;                             ///< 后台写入工作线程。
    std::mutex m_queueMutex;                           ///< 保护写入队列的互斥锁。
    std::condition_variable_any m_queueCv;             ///< 写入队列条件变量。
    std::deque<Request> m_queue;                       ///< 待写入请求队列。
};

}  // namespace Dss::Storage
