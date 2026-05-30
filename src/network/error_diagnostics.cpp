#include "dss/network/error_diagnostics.h"

#include <chrono>
#include <string>
#include <thread>

namespace Dss::Network
{

ErrorDiagnostics::ErrorDiagnostics(MessageBus& bus)
    : m_bus(bus)
{
}

ErrorDiagnostics::~ErrorDiagnostics()
{
    close();
}

auto ErrorDiagnostics::open(const UdpEndpointConfig& config) -> std::expected<void, std::string>
{
    auto result = m_channel.bind(config);
    if (!result)
    {
        return result;
    }

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
    return {};
}

void ErrorDiagnostics::close()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_channel.close();
}

void ErrorDiagnostics::workerLoop(std::stop_token token)
{
    using namespace std::chrono;

    while (!token.stop_requested())
    {
        // TODO: collect status from all comm/grabber/storage modules via ServiceRegistry
        // and build JSON diagnostic payload
        std::string json = R"({"status":"ok"})";
        auto data = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(json.data()), json.size());
        m_channel.send(data);

        std::this_thread::sleep_for(1s);
    }
}

} // namespace Dss::Network
