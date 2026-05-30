#pragma once

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

#include <expected>
#include <span>

namespace Dss::Network
{

class DataExchange
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit DataExchange(MessageBus& bus);

    auto open(const UdpEndpointConfig& gxtcConfig, const UdpEndpointConfig& gdclConfig)
        -> std::expected<void, std::string>;
    void close();

    void sendGxtc(std::span<const uint8_t> data);
    void sendGdcl(std::span<const uint8_t> data);

private:
    MessageBus& m_bus;
    UdpChannel m_gxtcChannel;
    UdpChannel m_gdclChannel;
};

} // namespace Dss::Network
