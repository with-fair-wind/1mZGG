#pragma once

#include <algorithm>
#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>

#include "dss/core/diagnostics/resource_snapshot.h"

namespace Dss::Processing {

/**
 * @brief 有界通道，线程安全的固定容量环形缓冲区
 * @tparam T 元素类型
 * @tparam Capacity 最大元素数量（默认为 4）
 */
template <typename T, size_t Capacity = 4>
class BoundedChannel {
public:
    /**
     * @brief 非阻塞入队
     * @param value 待入队元素
     * @param bytes 本元素保留的载荷字节数，用于资源计量。
     * @return 通道已满或已关闭时返回 false
     */
    bool tryPush(T value, std::size_t bytes = 0) {
        std::lock_guard lock(m_mutex);
        if (m_closed || m_count >= Capacity) {
            return false;
        }
        m_buffer[m_tail] = std::move(value);
        m_payloadBytes[m_tail] = bytes;  ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
        m_queuedBytes += bytes;          ///< 当前队列载荷字节总数。
        m_peakQueuedBytes =
            (std::max)(m_peakQueuedBytes, m_queuedBytes);  ///< 实例生命周期内队列载荷峰值。
        m_tail = (m_tail + 1) % Capacity;
        ++m_count;
        m_notEmpty.notify_one();
        return true;
    }

    /**
     * @brief 阻塞入队，通道满时等待直至有空位或停止
     * @param value 待入队元素
     * @param token 停止令牌，用于取消等待
     * @param bytes 本元素保留的载荷字节数，用于资源计量。
     * @return 成功入队返回 true；已停止或通道已关闭返回 false
     */
    bool push(T value, std::stop_token token, std::size_t bytes = 0) {
        std::unique_lock lock(m_mutex);
        if (!m_notFull.wait(lock, token, [this]() { return m_closed || m_count < Capacity; })) {
            return false;
        }
        if (m_closed) {
            return false;
        }
        m_buffer[m_tail] = std::move(value);
        m_payloadBytes[m_tail] = bytes;  ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
        m_queuedBytes += bytes;          ///< 当前队列载荷字节总数。
        m_peakQueuedBytes =
            (std::max)(m_peakQueuedBytes, m_queuedBytes);  ///< 实例生命周期内队列载荷峰值。
        m_tail = (m_tail + 1) % Capacity;
        ++m_count;
        m_notEmpty.notify_one();
        return true;
    }

    /**
     * @brief 阻塞出队，通道空时等待直至有元素或停止
     * @param token 停止令牌，用于取消等待
     * @return 取出的元素；已停止或通道已关闭且为空时返回 std::nullopt
     */
    auto pop(std::stop_token token) -> std::optional<T> {
        std::unique_lock lock(m_mutex);
        if (!m_notEmpty.wait(lock, token, [this]() { return m_closed || m_count > 0; })) {
            return std::nullopt;
        }
        if (m_count == 0) {
            return std::nullopt;
        }
        T value = std::move(m_buffer[m_head]);
        m_queuedBytes -= m_payloadBytes[m_head];  ///< 当前队列载荷字节总数。
        m_payloadBytes[m_head] = 0;               ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
        m_head = (m_head + 1) % Capacity;
        --m_count;
        m_notFull.notify_one();
        return value;
    }

    /**
     * @brief 非阻塞出队。
     * @return 取出的元素；通道为空时返回 std::nullopt。
     */
    [[nodiscard]] auto tryPop() -> std::optional<T> {
        std::lock_guard lock(m_mutex);
        if (m_count == 0) {
            return std::nullopt;
        }
        T value = std::move(m_buffer[m_head]);
        m_queuedBytes -= m_payloadBytes[m_head];  ///< 当前队列载荷字节总数。
        m_payloadBytes[m_head] = 0;               ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
        m_head = (m_head + 1) % Capacity;
        --m_count;
        m_notFull.notify_one();
        return value;
    }

    /** @brief 获取当前元素数量。 @return 通道内尚未取出的元素数。 */
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_count;
    }

    /** @brief 查询通道是否为空。 @return 没有待取元素时返回 true。 */
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(m_mutex);
        return m_count == 0;
    }

    /** @brief 获取本组件的资源采样。 @return 同一队列锁内采样的元素数和载荷字节指标。 */
    [[nodiscard]] auto resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
        std::lock_guard lock(m_mutex);
        Dss::Core::ResourceSnapshot result{};
        result.queuedItems = m_count;
        result.queuedBytes = m_queuedBytes;          ///< 当前队列载荷字节总数。
        result.peakQueuedBytes = m_peakQueuedBytes;  ///< 实例生命周期内队列载荷峰值。
        return result;
    }

    /// 清空队列并唤醒所有等待线程
    void clear() {
        std::lock_guard lock(m_mutex);
        for (auto& value : m_buffer) {
            value = T{};
        }
        m_head = 0;
        m_tail = 0;
        m_count = 0;
        m_queuedBytes = 0;       ///< 当前队列载荷字节总数。
        m_payloadBytes.fill(0);  ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    /// 关闭通道，唤醒所有等待线程；关闭后 push 失败，pop 在排空后返回空
    void close() {
        std::lock_guard lock(m_mutex);
        m_closed = true;
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    /// 重新打开通道，允许继续入队
    void open() {
        std::lock_guard lock(m_mutex);
        m_closed = false;
    }

private:
    mutable std::mutex m_mutex;              ///< 保护缓冲区及状态
    std::condition_variable_any m_notEmpty;  ///< 队列非空条件变量
    std::condition_variable_any m_notFull;   ///< 队列未满条件变量
    std::array<T, Capacity> m_buffer{};      ///< 环形缓冲区
    std::array<std::size_t, Capacity>
        m_payloadBytes{};                 ///< 各槽位对应的载荷字节数，由 m_mutex 保护。
    std::uint64_t m_queuedBytes = 0;      ///< 当前队列载荷字节总数。
    std::uint64_t m_peakQueuedBytes = 0;  ///< 实例生命周期内队列载荷峰值。
    size_t m_head = 0;                    ///< 读指针
    size_t m_tail = 0;                    ///< 写指针
    size_t m_count = 0;                   ///< 当前元素数量
    bool m_closed = false;                ///< 通道是否已关闭
};

}  // namespace Dss::Processing
