#pragma once

#include "dss/core/types.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

namespace Dss::Acquisition
{

class IFrameSource
{
public:
    virtual ~IFrameSource() = default;

    virtual auto init() -> std::expected<void, std::string> = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;

    [[nodiscard]] virtual auto frameWidth() const -> uint32_t = 0;
    [[nodiscard]] virtual auto frameHeight() const -> uint32_t = 0;
};

} // namespace Dss::Acquisition
