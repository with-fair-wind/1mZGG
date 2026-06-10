#include "dss/network/udp_channel.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QObject>
#include <QString>
#include <QUdpSocket>
#include <vector>

namespace Dss::Network {

UdpChannel::UdpChannel() = default;

UdpChannel::~UdpChannel() {
    close();
}

auto UdpChannel::bind(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    m_config = config;
    m_socket = std::make_unique<QUdpSocket>();

    QHostAddress addr;
    if (m_config.localIp.empty()) {
        addr = QHostAddress::Any;
    } else {
        addr = QHostAddress(QString::fromStdString(m_config.localIp));
    }

    if (m_config.localPort > 0) {
        if (!m_socket->bind(addr, m_config.localPort)) {
            return std::unexpected("Failed to bind UDP: " + m_socket->errorString().toStdString());
        }
    }

    QObject::connect(m_socket.get(), &QUdpSocket::readyRead, [this]() { onReadyRead(); });

    return {};
}

void UdpChannel::close() {
    if (m_socket) {
        m_socket->close();
        m_socket.reset();
    }
}

auto UdpChannel::send(std::span<const uint8_t> data) -> int64_t {
    return sendTo(data, m_config.remoteIp, m_config.remotePort);
}

auto UdpChannel::sendTo(std::span<const uint8_t> data, const std::string& host, uint16_t port)
    -> int64_t {
    if (!m_socket) {
        return -1;
    }
    return m_socket->writeDatagram(reinterpret_cast<const char*>(data.data()),
                                   static_cast<qint64>(data.size()),
                                   QHostAddress(QString::fromStdString(host)), port);
}

bool UdpChannel::isBound() const {
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void UdpChannel::setReceiveCallback(
    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb) {
    std::lock_guard lock(m_mutex);
    m_recvCallback = std::move(cb);
}

/**
 * @brief 处理 Qt readyRead 信号，循环读取待处理数据报并触发回调
 */
void UdpChannel::onReadyRead() {
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        if (datagram.isValid()) {
            decltype(m_recvCallback) callback;
            {
                std::lock_guard lock(m_mutex);
                callback = m_recvCallback;
            }

            if (callback) {
                const auto bytes = datagram.data();
                std::vector<uint8_t> payload(
                    reinterpret_cast<const uint8_t*>(bytes.constData()),
                    reinterpret_cast<const uint8_t*>(bytes.constData()) + bytes.size());
                const auto sender = datagram.senderAddress().toString().toStdString();
                const auto port = static_cast<uint16_t>(datagram.senderPort());
                callback(std::span<const uint8_t>(payload.data(), payload.size()), sender, port);
            }
        }
    }
}

}  // namespace Dss::Network
