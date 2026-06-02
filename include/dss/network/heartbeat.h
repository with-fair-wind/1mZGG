#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <thread>

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

class Heartbeat {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit Heartbeat(MessageBus& bus);
    ~Heartbeat();

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();
    [[nodiscard]] bool isOpen() const;
    void sendCloseGuard();

    [[nodiscard]] static auto buildFrame() -> std::array<uint8_t, 10>;
    [[nodiscard]] static auto buildCloseGuardFrame() -> std::array<uint8_t, 10>;

private:
    void workerLoop(std::stop_token token);

    [[maybe_unused]] MessageBus& m_bus;
    UdpChannel m_channel;
    std::jthread m_workerThread;
};

}  // namespace Dss::Network
