#include "dss/app/application_context.h"

#include <exception>
#include <string>

#include "dss/core/logger.h"

namespace Dss::App {

ApplicationContext::ApplicationContext() {
    m_bus.setExceptionHandler([](std::exception_ptr error) {
        std::string message = "unknown exception";
        try {
            std::rethrow_exception(std::move(error));
        } catch (const std::exception& exception) {
            message = exception.what();
        } catch (...) {
        }
        Dss::Core::Logger::instance().error("Event subscriber failed: {}", message);
    });
}

ApplicationContext::~ApplicationContext() {
    shutdown();  // 兜底:确保后台 worker 停止(即使上层未在 exec() 后显式调用)
    Dss::Core::Logger::instance().setBus(nullptr);
}

auto ApplicationContext::bus() -> MessageBus& {
    return m_bus;
}

auto ApplicationContext::registry() -> Dss::Core::ServiceRegistry& {
    return m_registry;
}

void ApplicationContext::wireLogger() {
    Dss::Core::Logger::instance().setBus(&m_bus);
}

auto ApplicationContext::loadConfig(const std::filesystem::path& configPath)
    -> std::expected<void, std::string> {
    auto& config = Dss::Core::Config::instance();
    auto loaded = config.load(configPath);
    if (!loaded) {
        return loaded;
    }
    const auto& logging = config.logging();
    if (logging.enabled) {
        const auto configured = Dss::Core::Logger::instance().configureRotatingFileLogging(
            logging.filePath, logging.maxFileSizeBytes, logging.maxFiles);
        if (!configured) {
            Dss::Core::Logger::instance().warn("File logging disabled: {}", configured.error());
        }
    }
    return {};
}

void ApplicationContext::shutdown() {
    // 退订不能等待 COW 快照中的回调。先 join 所有生产者，期间保持订阅者存活。
    if (m_stopServices) {
        m_stopServices();
    }
    m_connections.clear();
    m_registry.clear();
    m_stopServices = {};
}

}  // namespace Dss::App
