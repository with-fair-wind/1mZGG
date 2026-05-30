#include "dss/network/atmos_receiver.h"

#include "dss/core/events.h"

#include <cstring>

namespace Dss::Network
{

AtmosReceiver::AtmosReceiver(MessageBus& bus)
    : m_bus(bus)
{
}

auto AtmosReceiver::open(const UdpEndpointConfig& config) -> std::expected<void, std::string>
{
    auto result = m_channel.bind(config);
    if (!result)
    {
        return result;
    }

    m_channel.setReceiveCallback(
        [this](std::span<const uint8_t> data, const std::string& sender, uint16_t port) {
            onData(data, sender, port);
        });

    return {};
}

void AtmosReceiver::close()
{
    m_channel.close();
}

void AtmosReceiver::onData(std::span<const uint8_t> data, const std::string& /*sender*/, uint16_t /*port*/)
{
    // Original: #pragma pack(1) struct { int head; double temp, pressure, humidity; }
    constexpr size_t expectedSize = sizeof(int) + 3 * sizeof(double);
    if (data.size() < expectedSize)
    {
        return;
    }

    double temp = 0;
    double pressure = 0;
    double humidity = 0;
    std::memcpy(&temp, data.data() + sizeof(int), sizeof(double));
    std::memcpy(&pressure, data.data() + sizeof(int) + sizeof(double), sizeof(double));
    std::memcpy(&humidity, data.data() + sizeof(int) + 2 * sizeof(double), sizeof(double));

    m_bus.emit(Dss::Core::AtmosphereDataEvent{temp, pressure, humidity});
}

} // namespace Dss::Network
