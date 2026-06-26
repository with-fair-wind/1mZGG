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

    /**
     * @brief 获取全局唯一日志实例。
     * @return 全局日志门面的引用。
     */
    static auto instance() -> Logger&;

    /**
     * @brief 绑定用于发布日志事件的消息总线。
     * @param bus 消息总线的非拥有指针；传入 nullptr 可解除绑定。
     */
    void setBus(MessageBus* bus);

    /**
     * @brief 启用写入单个文件的日志后端。
     * @param path 日志文件路径。
     * @return 配置成功时为空；失败时包含错误描述。
     */
    auto enableFileLogging(const std::filesystem::path& path) -> std::expected<void, std::string>;

    /**
     * @brief 配置按大小轮转的文件日志后端。
     * @param path 日志文件路径。
     * @param maxFileSizeBytes 单个日志文件的最大字节数。
     * @param maxFiles 保留的轮转文件数量。
     * @return 配置成功时为空；失败时包含错误描述。
     */
    auto configureRotatingFileLogging(const std::filesystem::path& path,
                                      std::size_t maxFileSizeBytes, std::size_t maxFiles)
        -> std::expected<void, std::string>;

    /**
     * @brief 获取当前文件日志路径。
     * @return 文件日志未启用时为空路径，否则返回已配置路径。
     */
    [[nodiscard]] auto fileLogPath() const -> std::filesystem::path;

    /**
     * @brief 输出 INFO 级别日志。
     * @param msg 日志文本。
     */
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

    /**
     * @brief 输出 WARN 级别日志。
     * @param msg 日志文本。
     */
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

    /**
     * @brief 输出 ERROR 级别日志。
     * @param msg 日志文本。
     */
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
    /// @brief 创建并配置全局日志后端。
    Logger();
    /// @brief 释放日志后端资源。
    ~Logger();
    Logger(const Logger&) = delete;
    auto operator=(const Logger&) -> Logger& = delete;

    /**
     * @brief 通过消息总线发布日志事件。
     * @param level 日志级别。
     * @param message 日志文本。
     */
    void emitLog(LogLevel level, std::string message);

    std::shared_ptr<spdlog::logger> m_logger;         ///< spdlog 后端日志器
    std::shared_ptr<spdlog::sinks::sink> m_fileSink;  ///< 当前文件日志 sink
    std::filesystem::path m_fileLogPath;              ///< 当前文件日志路径
    MessageBus* m_bus = nullptr;                      ///< 绑定的消息总线（非拥有）
    mutable std::mutex m_mutex;                       ///< 保护 m_bus 的互斥锁
    mutable std::mutex m_sinkMutex;                   ///< 保护 sink 配置与日志写入
};

}  // namespace Dss::Core
