#include "dss/network/error_diagnostics.h"

#include <chrono>
#include <thread>

namespace Dss::Network {

ErrorDiagnostics::ErrorDiagnostics(MessageBus& bus) : m_bus(bus) {
    const auto markSerialFailure = [this](const std::string& channel) {
        std::scoped_lock lock(m_statusMutex);
        if (channel == "display") {
            m_status.displayCommOk = false;
        } else if (channel == "exposure") {
            m_status.exposureCommOk = false;
        }
    };
    m_connections.push_back(m_bus.subscribe<Dss::Core::SerialFrameErrorEvent>(
        [markSerialFailure](const auto& event) { markSerialFailure(event.channel); }));
    m_connections.push_back(m_bus.subscribe<Dss::Core::SerialDecodeErrorEvent>(
        [markSerialFailure](const auto& event) { markSerialFailure(event.channel); }));
    m_connections.push_back(m_bus.subscribe<Dss::Core::StorageWriteErrorEvent>([this](const auto&) {
        std::scoped_lock lock(m_statusMutex);
        m_status.storageOk = false;
    }));
    m_connections.push_back(
        m_bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>([this](const auto&) {
            std::scoped_lock lock(m_statusMutex);
            m_status.reservedOk = false;
        }));
}

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

auto ErrorDiagnostics::status() const -> Dss::Core::Status {
    return isOpen() ? Dss::Core::Status::Ok : Dss::Core::Status::Init;
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
