#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "dss/core/i_service.h"

namespace Dss::Core {

class ServiceHost {
public:
    [[nodiscard]] bool add(std::shared_ptr<IService> service);
    [[nodiscard]] auto serviceCount() const -> std::size_t;
    auto startAll() -> std::expected<void, std::string>;
    void stopAll() noexcept;

private:
    std::vector<std::shared_ptr<IService>> m_services;
    std::size_t m_startedCount = 0;
    bool m_running = false;
};

}  // namespace Dss::Core
