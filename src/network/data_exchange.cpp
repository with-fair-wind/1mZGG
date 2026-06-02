#include "dss/network/data_exchange.h"

namespace Dss::Network {

DataExchange::DataExchange(MessageBus& bus) : m_bus(bus) {}

auto DataExchange::open(const UdpEndpointConfig& gxtcConfig, const UdpEndpointConfig& gdclConfig)
    -> std::expected<void, std::string> {
    if (auto r = m_gxtcChannel.bind(gxtcConfig); !r) {
        return r;
    }
    if (auto r = m_gdclChannel.bind(gdclConfig); !r) {
        return r;
    }
    return {};
}

void DataExchange::close() {
    m_gxtcChannel.close();
    m_gdclChannel.close();
}

bool DataExchange::isOpen() const {
    return m_gxtcChannel.isBound() && m_gdclChannel.isBound();
}

void DataExchange::sendGxtc(std::span<const uint8_t> data) {
    m_gxtcChannel.send(data);
}

void DataExchange::sendGxtc(const GxtcMetadata& metadata, std::span<const GxtcTarget> targets) {
    const auto packet = buildGxtcPacket(metadata, targets);
    m_gxtcChannel.send(packet);
}

void DataExchange::sendGdcl(std::span<const uint8_t> data) {
    m_gdclChannel.send(data);
}

void DataExchange::sendGdcl(const GdclMeasurement& measurement) {
    const auto packet = buildGdclPacket(measurement);
    m_gdclChannel.send(packet);
}

}  // namespace Dss::Network
