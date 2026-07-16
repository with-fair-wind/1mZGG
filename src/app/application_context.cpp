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
    // 1) 先退订组合根事件订阅:回调(communication_services.cpp 注册的 imageSender、trackDataStorage 等)
    //    持有的服务 shared_ptr 在此释放。退订后工作线程不再命中这些订阅。
    //    ViewModel 仅持有 registry 引用、不缓存服务 shared_ptr,故无其它引用妨碍下一步清理。
    m_connections.clear();
    // 2) 再清空服务注册表:触发服务析构级联——frame source 析构 stop 帧线程并释放 frame callback
    //    持有的 imageProcessor/localImageStorage 引用,随后它们析构 stop worker;
    //    其余服务直接析构 stop/close。幂等,重复调用安全。
    m_registry.clear();
}

}  // namespace Dss::App
