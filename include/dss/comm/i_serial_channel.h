#pragma once

#include "dss/core/constants.h"
#include "dss/core/config_types.h"

#include <cstddef>
#include <expected>
#include <string>

namespace Dss::Comm
{

using SerialConfig = Dss::Core::SerialConfig;

class ISerialChannel
{
public:
    virtual ~ISerialChannel() = default;

    virtual auto open(const SerialConfig& config) -> std::expected<void, std::string> = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    [[nodiscard]] virtual auto status() const -> Dss::Core::Status = 0;

    [[nodiscard]] virtual auto recvFrameSize() const -> size_t = 0;
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t = 0;
};

} // namespace Dss::Comm
