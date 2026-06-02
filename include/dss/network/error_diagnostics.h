#pragma once

#include <atomic>
#include <expected>
#include <mutex>
#include <stop_token>
#include <thread>

#include "dss/core/event_bus.h"
#include "dss/network/diagnostic_protocol.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

class ErrorDiagnostics {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit ErrorDiagnostics(MessageBus& bus);
    ~ErrorDiagnostics();

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();
    [[nodiscard]] bool isOpen() const;
    void setStatus(const DiagnosticStatus& status);

private:
    void workerLoop(std::stop_token token);
    [[nodiscard]] auto statusSnapshot() const -> DiagnosticStatus;

    [[maybe_unused]] MessageBus& m_bus;
    UdpChannel m_channel;
    std::jthread m_workerThread;
    mutable std::mutex m_statusMutex;
    DiagnosticStatus m_status{};
};

}  // namespace Dss::Network
