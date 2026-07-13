#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace Dss::Core {

/// @brief 使用 std::stop_token 提供可立即取消的定时等待。
class InterruptibleWait {
public:
    InterruptibleWait() = default;
    InterruptibleWait(const InterruptibleWait&) = delete;
    auto operator=(const InterruptibleWait&) -> InterruptibleWait& = delete;

    /**
     * @brief 等待指定时长或停止请求。
     * @tparam Rep 时长计数类型。
     * @tparam Period 时长周期类型。
     * @param token 工作线程停止令牌。
     * @param timeout 最大等待时长。
     * @return 收到停止请求时返回 true，正常超时时返回 false。
     */
    template <typename Rep, typename Period>
    [[nodiscard]] bool waitFor(std::stop_token token, std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock lock(m_mutex);
        m_condition.wait_for(lock, token, timeout, [] { return false; });
        return token.stop_requested();
    }

private:
    std::mutex m_mutex;                       ///< 保护条件变量等待状态
    std::condition_variable_any m_condition;  ///< 与停止令牌协作的条件变量
};

}  // namespace Dss::Core
