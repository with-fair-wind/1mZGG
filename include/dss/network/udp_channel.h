#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>

#include "dss/network/i_network_channel.h"

class QUdpSocket;

namespace Dss::Network {

class UdpChannel {
public:
    UdpChannel();
    ~UdpChannel();

    UdpChannel(const UdpChannel&) = delete;
    UdpChannel& operator=(const UdpChannel&) = delete;

    auto bind(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();

    auto send(std::span<const uint8_t> data) -> int64_t;
    auto sendTo(std::span<const uint8_t> data, const std::string& host, uint16_t port) -> int64_t;

    [[nodiscard]] bool isBound() const;

    void setReceiveCallback(
        std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb);

private:
    void onReadyRead();

    std::unique_ptr<QUdpSocket> m_socket;
    UdpEndpointConfig m_config{};
    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> m_recvCallback;
    std::mutex m_mutex;
};

}  // namespace Dss::Network
