#include "dss/network/heartbeat.h"

#include <chrono>
#include <thread>

#include "dss/core/constants.h"

namespace Dss::Network {

Heartbeat::Heartbeat(MessageBus& bus) : m_bus(bus) {}

Heartbeat::~Heartbeat() {
    close();
}

auto Heartbeat::open(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    auto result = m_channel.bind(config);
    if (!result) {
        return result;
    }

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
    return {};
}

void Heartbeat::close() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_channel.close();
}

bool Heartbeat::isOpen() const {
    return m_channel.isBound();
}

auto Heartbeat::status() const -> Dss::Core::Status {
    return isOpen() ? Dss::Core::Status::Ok : Dss::Core::Status::Init;
}

auto Heartbeat::buildFrame() -> std::array<uint8_t, 10> {
    std::array<uint8_t, 10> frame{};
    frame[0] = Dss::Core::FrameHeader;
    frame[9] = Dss::Core::FrameTail;
    return frame;
}

auto Heartbeat::buildCloseGuardFrame() -> std::array<uint8_t, 10> {
    auto frame = buildFrame();
    frame[1] = 0x01;
    return frame;
}

void Heartbeat::sendCloseGuard() {
    const auto frame = buildCloseGuardFrame();
    m_channel.send(frame);
}

void Heartbeat::workerLoop(std::stop_token token) {
    using namespace std::chrono;

    while (!token.stop_requested()) {
        const auto frame = buildFrame();
        m_channel.send(frame);

        std::this_thread::sleep_for(100ms);
    }
}

}  // namespace Dss::Network
