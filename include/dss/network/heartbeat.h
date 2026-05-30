#pragma once

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

#include <expected>
#include <stop_token>
#include <thread>

namespace Dss::Network
{

class Heartbeat
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit Heartbeat(MessageBus& bus);
    ~Heartbeat();

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();
    void sendCloseGuard();

private:
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;
    UdpChannel m_channel;
    std::jthread m_workerThread;
};

} // namespace Dss::Network
