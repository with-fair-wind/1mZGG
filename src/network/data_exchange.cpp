#include "dss/network/data_exchange.h"

#include <utility>

#include "dss/core/events.h"

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

auto DataExchange::sendGxtc(std::span<const uint8_t> data) -> std::expected<int64_t, std::string> {
    return sendPacket("gxtc", m_gxtcChannel, data);
}

auto DataExchange::sendGxtc(const GxtcMetadata& metadata, std::span<const GxtcTarget> targets)
    -> std::expected<int64_t, std::string> {
    const auto packet = buildGxtcPacket(metadata, targets);
    return sendGxtc(packet);
}

auto DataExchange::sendGdcl(std::span<const uint8_t> data) -> std::expected<int64_t, std::string> {
    return sendPacket("gdcl", m_gdclChannel, data);
}

auto DataExchange::sendGdcl(const GdclMeasurement& measurement)
    -> std::expected<int64_t, std::string> {
    const auto packet = buildGdclPacket(measurement);
    return sendGdcl(packet);
}

auto DataExchange::sendPacket(std::string_view channelName, UdpChannel& channel,
                              std::span<const uint8_t> data)
    -> std::expected<int64_t, std::string> {
    const auto sent = channel.send(data);
    if (sent >= 0) {
        return sent;
    }

    auto channelLabel = std::string(channelName);
    auto message = channelLabel + " UDP channel is not open or send failed";
    m_bus.emit(Dss::Core::NetworkTransmissionErrorEvent{
        .channel = std::move(channelLabel),
        .message = message,
        .attemptedBytes = static_cast<uint64_t>(data.size()),
    });
    return std::unexpected(std::move(message));
}

}  // namespace Dss::Network
