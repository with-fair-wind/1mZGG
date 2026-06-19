#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/core/logger.h"

namespace {

/// @brief 在测试生命周期内临时绑定 Logger 的事件总线。
class ScopedLoggerBus {
public:
    /// @brief 绑定指定消息总线。
    explicit ScopedLoggerBus(Dss::Core::Logger::MessageBus& bus) {
        Dss::Core::Logger::instance().setBus(&bus);
    }

    /// @brief 解除测试绑定，避免影响后续用例。
    ~ScopedLoggerBus() {
        Dss::Core::Logger::instance().setBus(nullptr);
    }

    ScopedLoggerBus(const ScopedLoggerBus&) = delete;
    auto operator=(const ScopedLoggerBus&) -> ScopedLoggerBus& = delete;
};

[[nodiscard]] auto tempLogDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_logger_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto readAllText(const std::filesystem::path& path) -> std::string {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}  // namespace

TEST(LoggerTest, PublishesFormattedMessagesToEventBus) {
    Dss::Core::Logger::MessageBus bus;
    std::vector<Dss::Core::LogMessageEvent> messages;
    auto connection = bus.subscribe<Dss::Core::LogMessageEvent>(
        [&messages](const Dss::Core::LogMessageEvent& event) { messages.push_back(event); });
    const ScopedLoggerBus scopedBus(bus);

    auto& logger = Dss::Core::Logger::instance();
    logger.info("logger ready");
    logger.warn("serial {} timeout", "display");
    logger.error("udp port {} send failed", 7000);
    logger.flush();

    ASSERT_EQ(messages.size(), 3U);
    EXPECT_EQ(messages[0].level, Dss::Core::LogLevel::Info);
    EXPECT_EQ(messages[0].message, "[INFO] logger ready");
    EXPECT_EQ(messages[1].level, Dss::Core::LogLevel::Warning);
    EXPECT_EQ(messages[1].message, "[WARN] serial display timeout");
    EXPECT_EQ(messages[2].level, Dss::Core::LogLevel::Error);
    EXPECT_EQ(messages[2].message, "[ERROR] udp port 7000 send failed");
}

TEST(LoggerTest, WritesMessagesToConfiguredFile) {
    const auto dir = tempLogDir();
    const auto logPath = dir / "dss.log";

    auto& logger = Dss::Core::Logger::instance();
    const auto enabled = logger.enableFileLogging(logPath);
    ASSERT_TRUE(enabled.has_value()) << enabled.error();

    logger.info("file logger ready");
    logger.warn("file logger warning");
    logger.error("file logger error");
    logger.flush();

    ASSERT_TRUE(std::filesystem::exists(logPath));
    const auto text = readAllText(logPath);
    EXPECT_NE(text.find("file logger ready"), std::string::npos);
    EXPECT_NE(text.find("file logger warning"), std::string::npos);
    EXPECT_NE(text.find("file logger error"), std::string::npos);
}