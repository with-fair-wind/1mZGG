#include "dss/network/heartbeat.h"

#include "dss/core/constants.h"

#include <array>
#include <chrono>
#include <thread>

namespace Dss::Network
{

Heartbeat::Heartbeat(MessageBus& bus)
    : m_bus(bus)
{
}

Heartbeat::~Heartbeat()
{
    close();
}

auto Heartbeat::open(const UdpEndpointConfig& config) -> std::expected<void, std::string>
{
    auto result = m_channel.bind(config);
    if (!result)
    {
        return result;
    }

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
    return {};
}

void Heartbeat::close()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_channel.close();
}

void Heartbeat::sendCloseGuard()
{
    std::array<uint8_t, 10> frame{};
    frame[0] = Dss::Core::FrameHeader;
    frame[9] = Dss::Core::FrameTail;
    frame[1] = 0xFF; // close guard marker
    m_channel.send(frame);
}

void Heartbeat::workerLoop(std::stop_token token)
{
    using namespace std::chrono;

    while (!token.stop_requested())
    {
        std::array<uint8_t, 10> frame{};
        frame[0] = Dss::Core::FrameHeader;
        frame[9] = Dss::Core::FrameTail;
        m_channel.send(frame);

        std::this_thread::sleep_for(100ms);
    }
}

} // namespace Dss::Network
