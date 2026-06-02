#pragma once

#include <expected>
#include <span>

#include "dss/core/event_bus.h"
#include "dss/network/data_exchange_protocol.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

class DataExchange {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit DataExchange(MessageBus& bus);

    auto open(const UdpEndpointConfig& gxtcConfig, const UdpEndpointConfig& gdclConfig)
        -> std::expected<void, std::string>;
    void close();
    [[nodiscard]] bool isOpen() const;

    void sendGxtc(std::span<const uint8_t> data);
    void sendGxtc(const GxtcMetadata& metadata, std::span<const GxtcTarget> targets);
    void sendGdcl(std::span<const uint8_t> data);
    void sendGdcl(const GdclMeasurement& measurement);

private:
    [[maybe_unused]] MessageBus& m_bus;
    UdpChannel m_gxtcChannel;
    UdpChannel m_gdclChannel;
};

}  // namespace Dss::Network
