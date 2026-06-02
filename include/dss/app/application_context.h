#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "dss/core/config.h"
#include "dss/core/event_bus.h"
#include "dss/core/service_host.h"
#include "dss/core/service_registry.h"

namespace Dss::App {

class ApplicationContext {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    ApplicationContext();
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&) = delete;
    ApplicationContext& operator=(ApplicationContext&&) = delete;

    [[nodiscard]] auto bus() -> MessageBus&;
    [[nodiscard]] auto registry() -> Dss::Core::ServiceRegistry&;
    [[nodiscard]] auto services() -> Dss::Core::ServiceHost&;

    void wireLogger();
    auto loadConfig(const std::filesystem::path& configPath) -> std::expected<void, std::string>;
    void registerCommunicationServices();
    auto startServices() -> std::expected<void, std::string>;
    void stopServices() noexcept;

private:
    MessageBus m_bus;
    Dss::Core::ServiceRegistry m_registry;
    Dss::Core::ServiceHost m_services;
};

}  // namespace Dss::App
