#pragma once

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

#include <atomic>
#include <expected>
#include <stop_token>
#include <thread>

namespace Dss::Network
{

class ErrorDiagnostics
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit ErrorDiagnostics(MessageBus& bus);
    ~ErrorDiagnostics();

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();

private:
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;
    UdpChannel m_channel;
    std::jthread m_workerThread;
};

} // namespace Dss::Network
