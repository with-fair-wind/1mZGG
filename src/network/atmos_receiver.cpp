#include "dss/network/atmos_receiver.h"

#include "dss/core/events.h"
#include "dss/network/atmos_protocol.h"

namespace Dss::Network {

AtmosReceiver::AtmosReceiver(MessageBus& bus) : m_bus(bus) {}

auto AtmosReceiver::open(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    auto result = m_channel.bind(config);
    if (!result) {
        return result;
    }

    m_channel.setReceiveCallback([this](std::span<const uint8_t> data, const std::string& sender,
                                        uint16_t port) { onData(data, sender, port); });

    return {};
}

void AtmosReceiver::close() {
    m_channel.close();
}

bool AtmosReceiver::isOpen() const {
    return m_channel.isBound();
}

auto AtmosReceiver::status() const -> Dss::Core::Status {
    return isOpen() ? Dss::Core::Status::Ok : Dss::Core::Status::Init;
}

void AtmosReceiver::onData(std::span<const uint8_t> data, const std::string& /*sender*/,
                           uint16_t /*port*/) {
    const auto sample = decodeAtmosPacket(data);
    if (!sample) {
        return;
    }

    m_bus.emit(Dss::Core::AtmosphereDataEvent{
        .temperature = sample->temperature,
        .pressure = sample->pressure,
        .humidity = sample->humidity,
    });
}

}  // namespace Dss::Network
