#pragma once

#include "dss/core/constants.h"

#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace Dss::Comm
{

struct SerialConfig
{
    std::string portName;
    int baudRate = 230400;
    int dataBits = 8;
    int stopBits = 1;
};

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
