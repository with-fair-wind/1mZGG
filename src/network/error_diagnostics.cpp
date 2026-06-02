#include "dss/network/error_diagnostics.h"

#include <chrono>
#include <thread>

namespace Dss::Network {

ErrorDiagnostics::ErrorDiagnostics(MessageBus& bus) : m_bus(bus) {}

ErrorDiagnostics::~ErrorDiagnostics() {
    close();
}

auto ErrorDiagnostics::open(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    auto result = m_channel.bind(config);
    if (!result) {
        return result;
    }

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
    return {};
}

void ErrorDiagnostics::close() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_channel.close();
}

bool ErrorDiagnostics::isOpen() const {
    return m_channel.isBound();
}

void ErrorDiagnostics::setStatus(const DiagnosticStatus& status) {
    std::scoped_lock lock(m_statusMutex);
    m_status = status;
}

auto ErrorDiagnostics::statusSnapshot() const -> DiagnosticStatus {
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

void ErrorDiagnostics::workerLoop(std::stop_token token) {
    using namespace std::chrono;

    while (!token.stop_requested()) {
        const auto json = buildDiagnosticStatusJson(statusSnapshot());
        auto data =
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size());
        m_channel.send(data);

        std::this_thread::sleep_for(1s);
    }
}

}  // namespace Dss::Network
