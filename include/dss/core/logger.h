#pragma once

#include <format>
#include <mutex>
#include <string>
#include <string_view>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"

namespace Dss::Core {

/// 全局日志单例，通过事件总线发布 LogMessageEvent
class Logger {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 消息总线类型

    /// 获取全局唯一实例
    static auto instance() -> Logger& {
        static Logger logger;
        return logger;
    }

    /// 绑定消息总线（用于发布日志事件）
    void setBus(MessageBus* bus) {
        std::lock_guard lock(m_mutex);
        m_bus = bus;
    }

    /// 输出 INFO 级别日志
    void info(std::string_view msg) {
        emitLog(std::string(msg));
    }

    /**
     * @brief 输出格式化的 INFO 级别日志
     * @tparam Args 格式化参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        emitLog(std::format(fmt, std::forward<Args>(args)...));
    }

    /// 输出 WARN 级别日志
    void warn(std::string_view msg) {
        emitLog(std::format("[WARN] {}", msg));
    }

    /// 输出 ERROR 级别日志
    void error(std::string_view msg) {
        emitLog(std::format("[ERROR] {}", msg));
    }

private:
    Logger() = default;

    /// 通过消息总线发布日志事件
    void emitLog(std::string message) {
        MessageBus* bus = nullptr;
        {
            std::lock_guard lock(m_mutex);
            bus = m_bus;
        }

        if (bus) {
            bus->emit(LogMessageEvent{std::move(message)});
        }
    }

    MessageBus* m_bus = nullptr;  ///< 绑定的消息总线（非拥有）
    std::mutex m_mutex;           ///< 保护 m_bus 的互斥锁
};

}  // namespace Dss::Core
