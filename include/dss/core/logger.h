#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"

namespace spdlog {
class logger;
namespace sinks {
class sink;
}  // namespace sinks
}  // namespace spdlog

namespace Dss::Core {

/// 全局日志门面，通过 spdlog 写入后端并向事件总线发布 LogMessageEvent
class Logger {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 消息总线类型

    /// 获取全局唯一实例
    static auto instance() -> Logger&;

    /// 绑定消息总线（用于发布日志事件）
    void setBus(MessageBus* bus);

    auto enableFileLogging(const std::filesystem::path& path) -> std::expected<void, std::string>;
    auto configureRotatingFileLogging(const std::filesystem::path& path,
                                      std::size_t maxFileSizeBytes, std::size_t maxFiles)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto fileLogPath() const -> std::filesystem::path;

    /// 输出 INFO 级别日志
    void info(std::string_view msg);

    /**
     * @brief 输出格式化的 INFO 级别日志
     * @tparam Args 格式化参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        info(std::format(fmt, std::forward<Args>(args)...));
    }

    /// 输出 WARN 级别日志
    void warn(std::string_view msg);

    /**
     * @brief 输出格式化的 WARN 级别日志
     * @tparam Args 格式化参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        warn(std::format(fmt, std::forward<Args>(args)...));
    }

    /// 输出 ERROR 级别日志
    void error(std::string_view msg);

    /**
     * @brief 输出格式化的 ERROR 级别日志
     * @tparam Args 格式化参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        error(std::format(fmt, std::forward<Args>(args)...));
    }

    /// 刷新 spdlog 后端缓冲区
    void flush();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    auto operator=(const Logger&) -> Logger& = delete;

    /// 通过消息总线发布日志事件
    void emitLog(LogLevel level, std::string message);

    std::shared_ptr<spdlog::logger> m_logger;  ///< spdlog 后端日志器
    std::shared_ptr<spdlog::sinks::sink> m_fileSink;
    std::filesystem::path m_fileLogPath;
    MessageBus* m_bus = nullptr;     ///< 绑定的消息总线（非拥有）
    mutable std::mutex m_mutex;      ///< 保护 m_bus 的互斥锁
    mutable std::mutex m_sinkMutex;  ///< 保护 sink 配置与日志写入
};

}  // namespace Dss::Core
