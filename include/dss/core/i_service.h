#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace Dss::Core {

class IService {
public:
    virtual ~IService() = default;

    [[nodiscard]] virtual auto name() const -> std::string_view = 0;
    virtual auto start() -> std::expected<void, std::string> = 0;
    virtual void stop() noexcept = 0;
};

}  // namespace Dss::Core
