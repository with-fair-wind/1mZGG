#include "dss/network/udp_channel.h"

#include <QHostAddress>
#include <QNetworkDatagram>

namespace Dss::Network
{

UdpChannel::~UdpChannel()
{
    close();
}

auto UdpChannel::bind(const UdpEndpointConfig& config) -> std::expected<void, std::string>
{
    m_config = config;
    m_socket = std::make_unique<QUdpSocket>();

    QHostAddress addr;
    if (m_config.localIp.empty())
    {
        addr = QHostAddress::Any;
    }
    else
    {
        addr = QHostAddress(QString::fromStdString(m_config.localIp));
    }

    if (m_config.localPort > 0)
    {
        if (!m_socket->bind(addr, m_config.localPort))
        {
            return std::unexpected("Failed to bind UDP: " + m_socket->errorString().toStdString());
        }
    }

    QObject::connect(m_socket.get(), &QUdpSocket::readyRead, [this]() { onReadyRead(); });

    return {};
}

void UdpChannel::close()
{
    if (m_socket)
    {
        m_socket->close();
        m_socket.reset();
    }
}

auto UdpChannel::send(std::span<const uint8_t> data) -> int64_t
{
    return sendTo(data, m_config.remoteIp, m_config.remotePort);
}

auto UdpChannel::sendTo(std::span<const uint8_t> data, const std::string& host, uint16_t port) -> int64_t
{
    if (!m_socket)
    {
        return -1;
    }
    return m_socket->writeDatagram(
        reinterpret_cast<const char*>(data.data()),
        static_cast<qint64>(data.size()),
        QHostAddress(QString::fromStdString(host)),
        port);
}

bool UdpChannel::isBound() const
{
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void UdpChannel::setReceiveCallback(
    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb)
{
    std::lock_guard lock(m_mutex);
    m_recvCallback = std::move(cb);
}

void UdpChannel::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        if (datagram.isValid())
        {
            std::lock_guard lock(m_mutex);
            if (m_recvCallback)
            {
                auto bytes = datagram.data();
                auto sender = datagram.senderAddress().toString().toStdString();
                auto port = static_cast<uint16_t>(datagram.senderPort());
                m_recvCallback(
                    std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(bytes.constData()),
                        static_cast<size_t>(bytes.size())),
                    sender,
                    port);
            }
        }
    }
}

} // namespace Dss::Network
