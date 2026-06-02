#pragma once

#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>

namespace Dss::Processing {

template <typename T, size_t Capacity = 4>
class BoundedChannel {
public:
    bool tryPush(T value) {
        std::lock_guard lock(m_mutex);
        if (m_closed || m_count >= Capacity) {
            return false;
        }
        m_buffer[m_tail] = std::move(value);
        m_tail = (m_tail + 1) % Capacity;
        ++m_count;
        m_notEmpty.notify_one();
        return true;
    }

    bool push(T value, std::stop_token token) {
        std::unique_lock lock(m_mutex);
        if (!m_notFull.wait(lock, token, [this]() { return m_closed || m_count < Capacity; })) {
            return false;
        }
        if (m_closed) {
            return false;
        }
        m_buffer[m_tail] = std::move(value);
        m_tail = (m_tail + 1) % Capacity;
        ++m_count;
        m_notEmpty.notify_one();
        return true;
    }

    auto pop(std::stop_token token) -> std::optional<T> {
        std::unique_lock lock(m_mutex);
        if (!m_notEmpty.wait(lock, token, [this]() { return m_closed || m_count > 0; })) {
            return std::nullopt;
        }
        if (m_count == 0) {
            return std::nullopt;
        }
        T value = std::move(m_buffer[m_head]);
        m_head = (m_head + 1) % Capacity;
        --m_count;
        m_notFull.notify_one();
        return value;
    }

    [[nodiscard]] auto tryPop() -> std::optional<T> {
        std::lock_guard lock(m_mutex);
        if (m_count == 0) {
            return std::nullopt;
        }
        T value = std::move(m_buffer[m_head]);
        m_head = (m_head + 1) % Capacity;
        --m_count;
        m_notFull.notify_one();
        return value;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_count;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(m_mutex);
        return m_count == 0;
    }

    void clear() {
        std::lock_guard lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_count = 0;
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    void close() {
        std::lock_guard lock(m_mutex);
        m_closed = true;
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    void open() {
        std::lock_guard lock(m_mutex);
        m_closed = false;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable_any m_notEmpty;
    std::condition_variable_any m_notFull;
    std::array<T, Capacity> m_buffer{};
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_count = 0;
    bool m_closed = false;
};

}  // namespace Dss::Processing
