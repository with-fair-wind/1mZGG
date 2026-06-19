#include "dss/core/logger.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

/// @brief 将 spdlog 级别映射到核心日志级别。
[[nodiscard]] auto toCoreLogLevel(spdlog::level::level_enum level) -> Dss::Core::LogLevel {
    switch (level) {
        case spdlog::level::warn:
            return Dss::Core::LogLevel::Warning;
        case spdlog::level::err:
        case spdlog::level::critical:
            return Dss::Core::LogLevel::Error;
        case spdlog::level::trace:
        case spdlog::level::debug:
        case spdlog::level::info:
        case spdlog::level::off:
        case spdlog::level::n_levels:
            return Dss::Core::LogLevel::Info;
    }
    return Dss::Core::LogLevel::Info;
}

/// @brief 返回 UI 兼容的日志级别前缀。
[[nodiscard]] auto logPrefix(Dss::Core::LogLevel level) -> std::string_view {
    switch (level) {
        case Dss::Core::LogLevel::Warning:
            return "[WARN]";
        case Dss::Core::LogLevel::Error:
            return "[ERROR]";
        case Dss::Core::LogLevel::Info:
            return "[INFO]";
    }
    return "[INFO]";
}

/// @brief 将 spdlog 消息转为事件总线日志文本。
[[nodiscard]] auto makeEventMessage(Dss::Core::LogLevel level, const spdlog::details::log_msg& msg)
    -> std::string {
    std::string message;
    message.reserve(logPrefix(level).size() + 1U + msg.payload.size());
    message.append(logPrefix(level));
    message.push_back(' ');
    message.append(msg.payload.data(), msg.payload.size());
    return message;
}

/// @brief spdlog sink：把后端日志同步发布为核心日志事件。
class EventBusSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    using Callback = std::function<void(Dss::Core::LogLevel, std::string)>;  ///< 事件发布回调

    /// @brief 保存事件发布回调。
    explicit EventBusSink(Callback callback) : m_callback(std::move(callback)) {}

protected:
    /// @brief 接收 spdlog 消息并转为 LogMessageEvent。
    void sink_it_(const spdlog::details::log_msg& msg) override {
        const auto level = toCoreLogLevel(msg.level);
        m_callback(level, makeEventMessage(level, msg));
    }

    /// @brief 事件总线 sink 无额外缓冲，刷新为空操作。
    void flush_() override {}

private:
    Callback m_callback;  ///< 事件发布回调
};

}  // namespace

namespace Dss::Core {

auto Logger::instance() -> Logger& {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    auto eventSink = std::make_shared<EventBusSink>(
        [this](LogLevel level, std::string message) { emitLog(level, std::move(message)); });
    eventSink->set_level(spdlog::level::trace);

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    consoleSink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    std::vector<spdlog::sink_ptr> sinks{eventSink, consoleSink};
    m_logger = std::make_shared<spdlog::logger>("dss", sinks.begin(), sinks.end());
    m_logger->set_level(spdlog::level::trace);
    m_logger->flush_on(spdlog::level::err);
}

Logger::~Logger() = default;

void Logger::setBus(MessageBus* bus) {
    std::lock_guard lock(m_mutex);
    m_bus = bus;
}


auto Logger::enableFileLogging(const std::filesystem::path& path)
    -> std::expected<void, std::string> {
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return std::unexpected("failed to create log directory: " + error.message());
        }
    }

    try {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        fileSink->set_level(spdlog::level::trace);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        m_logger->sinks().push_back(std::move(fileSink));
    } catch (const spdlog::spdlog_ex& ex) {
        return std::unexpected(std::string{"failed to enable file logging: "} + ex.what());
    }

    return {};
}
void Logger::info(std::string_view msg) {
    m_logger->info("{}", msg);
}

void Logger::warn(std::string_view msg) {
    m_logger->warn("{}", msg);
}

void Logger::error(std::string_view msg) {
    m_logger->error("{}", msg);
}

void Logger::flush() {
    m_logger->flush();
}

void Logger::emitLog(LogLevel level, std::string message) {
    MessageBus* bus = nullptr;
    {
        std::lock_guard lock(m_mutex);
        bus = m_bus;
    }

    if (bus) {
        bus->emit(LogMessageEvent{.level = level, .message = std::move(message)});
    }
}

}  // namespace Dss::Core
