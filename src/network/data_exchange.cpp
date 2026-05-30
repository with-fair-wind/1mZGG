#include "dss/network/data_exchange.h"

namespace Dss::Network
{

DataExchange::DataExchange(MessageBus& bus)
    : m_bus(bus)
{
}

auto DataExchange::open(const UdpEndpointConfig& gxtcConfig, const UdpEndpointConfig& gdclConfig)
    -> std::expected<void, std::string>
{
    if (auto r = m_gxtcChannel.bind(gxtcConfig); !r)
    {
        return r;
    }
    if (auto r = m_gdclChannel.bind(gdclConfig); !r)
    {
        return r;
    }
    return {};
}

void DataExchange::close()
{
    m_gxtcChannel.close();
    m_gdclChannel.close();
}

void DataExchange::sendGxtc(std::span<const uint8_t> data)
{
    m_gxtcChannel.send(data);
}

void DataExchange::sendGdcl(std::span<const uint8_t> data)
{
    m_gdclChannel.send(data);
}

} // namespace Dss::Network
