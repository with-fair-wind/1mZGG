#pragma once

#include "dss/core/constants.h"
#include "dss/core/config_types.h"

#include <expected>
#include <string>

namespace Dss::Network
{

using UdpEndpointConfig = Dss::Core::UdpEndpointConfig;

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
