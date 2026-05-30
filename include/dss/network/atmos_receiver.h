#pragma once

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace Dss::Network
{

class AtmosReceiver
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit AtmosReceiver(MessageBus& bus);

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();

private:
    void onData(std::span<const uint8_t> data, const std::string& sender, uint16_t port);

    MessageBus& m_bus;
    UdpChannel m_channel;
};

} // namespace Dss::Network
