#include "dss/core/service_host.h"

#include <format>

namespace Dss::Core {

bool ServiceHost::add(std::shared_ptr<IService> service) {
    if (!service || m_running || m_startedCount > 0) {
        return false;
    }
    m_services.push_back(std::move(service));
    return true;
}

auto ServiceHost::serviceCount() const -> std::size_t {
    return m_services.size();
}

auto ServiceHost::startAll() -> std::expected<void, std::string> {
    if (m_running) {
        return {};
    }

    for (; m_startedCount < m_services.size(); ++m_startedCount) {
        auto& service = m_services[m_startedCount];
        if (auto result = service->start(); !result) {
            const auto serviceName = std::string(service->name());
            stopAll();
            return std::unexpected(
                std::format("Failed to start service '{}': {}", serviceName, result.error()));
        }
    }
    m_running = true;
    return {};
}

void ServiceHost::stopAll() noexcept {
    while (m_startedCount > 0) {
        --m_startedCount;
        m_services[m_startedCount]->stop();
    }
    m_running = false;
}

}  // namespace Dss::Core
