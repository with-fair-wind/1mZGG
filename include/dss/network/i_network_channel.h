#pragma once

#include "dss/core/constants.h"

#include <cstdint>
#include <expected>
#include <string>

namespace Dss::Network
{

struct UdpEndpointConfig
{
    std::string localIp;
    uint16_t localPort = 0;
    std::string remoteIp;
    uint16_t remotePort = 0;
};

class INetworkChannel
{
public:
    virtual ~INetworkChannel() = default;

    virtual auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    [[nodiscard]] virtual auto status() const -> Dss::Core::Status = 0;
};

} // namespace Dss::Network
