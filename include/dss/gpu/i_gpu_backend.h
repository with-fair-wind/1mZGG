#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace Dss::Gpu {

class IGpuBackend {
public:
    virtual ~IGpuBackend() = default;

    virtual auto init() -> std::expected<void, std::string> = 0;
    [[nodiscard]] virtual bool isInitialized() const = 0;
    [[nodiscard]] virtual auto deviceName() const -> std::string = 0;
};

}  // namespace Dss::Gpu
