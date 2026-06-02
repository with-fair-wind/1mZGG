#include "dss/app/application_context.h"

#include "dss/core/logger.h"

namespace Dss::App {

ApplicationContext::ApplicationContext() = default;

ApplicationContext::~ApplicationContext() {
    stopServices();
    Dss::Core::Logger::instance().setBus(nullptr);
}

auto ApplicationContext::bus() -> MessageBus& {
    return m_bus;
}

auto ApplicationContext::registry() -> Dss::Core::ServiceRegistry& {
    return m_registry;
}

auto ApplicationContext::services() -> Dss::Core::ServiceHost& {
    return m_services;
}

void ApplicationContext::wireLogger() {
    Dss::Core::Logger::instance().setBus(&m_bus);
}

auto ApplicationContext::loadConfig(const std::filesystem::path& configPath)
    -> std::expected<void, std::string> {
    return Dss::Core::Config::instance().load(configPath);
}

auto ApplicationContext::startServices() -> std::expected<void, std::string> {
    return m_services.startAll();
}

void ApplicationContext::stopServices() noexcept {
    m_services.stopAll();
}

}  // namespace Dss::App
