#include "dss/network/transport/udp_channel.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QString>
#include <QUdpSocket>
#include <chrono>
#include <optional>
#include <utility>

namespace Dss::Network {

namespace {

constexpr auto UdpPollInterval = std::chrono::milliseconds{5};
thread_local UdpChannel* CurrentUdpChannel = nullptr;

[[nodiscard]] auto bindAddress(const UdpEndpointConfig& config) -> QHostAddress {
    if (config.localIp.empty()) {
        return QHostAddress::Any;
    }
    return QHostAddress(QString::fromStdString(config.localIp));
}

[[nodiscard]] auto sendDatagram(QUdpSocket& socket, std::span<const uint8_t> data,
                                const std::string& host, uint16_t port) -> int64_t {
    return socket.writeDatagram(reinterpret_cast<const char*>(data.data()),
                                static_cast<qint64>(data.size()),
                                QHostAddress(QString::fromStdString(host)), port);
}

}  // namespace

UdpChannel::UdpChannel() = default;

UdpChannel::~UdpChannel() {
    close();
}

auto UdpChannel::bind(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
    {
        std::lock_guard sendLock(m_sendMutex);
        m_remoteIp = config.remoteIp;
        m_remotePort = config.remotePort;
    }

    std::promise<std::expected<void, std::string>> initPromise;
    auto initFuture = initPromise.get_future();
    m_workerThread = std::jthread(
        [this, config, promise = std::move(initPromise)](std::stop_token token) mutable {
            workerLoop(token, config, std::move(promise));
        });

    auto result = initFuture.get();
    if (!result) {
        if (m_workerThread.joinable()) {
            m_workerThread.request_stop();
            m_workerThread.join();
        }
        return result;
    }

    {
        std::lock_guard sendLock(m_sendMutex);
        m_acceptingSends = true;
    }
    return {};
}

void UdpChannel::close() {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
}

void UdpChannel::closeLocked() {
    {
        std::lock_guard sendLock(m_sendMutex);
        m_acceptingSends = false;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    failPendingSends();
    m_bound.store(false);
    m_localPort.store(0);
}

auto UdpChannel::send(std::span<const uint8_t> data) -> int64_t {
    std::string host;
    uint16_t port = 0;
    {
        std::lock_guard lock(m_sendMutex);
        host = m_remoteIp;
        port = m_remotePort;
    }
    return sendTo(data, host, port);
}

auto UdpChannel::sendTo(std::span<const uint8_t> data, const std::string& host, uint16_t port)
    -> int64_t {
    if (CurrentUdpChannel == this) {
        if (auto* socket = m_workerSocket.load(); socket != nullptr) {
            return sendDatagram(*socket, data, host, port);
        }
    }

    SendRequest request;
    request.data.assign(data.begin(), data.end());
    request.host = host;
    request.port = port;
    auto resultFuture = request.result.get_future();
    {
        std::lock_guard lock(m_sendMutex);
        if (!m_acceptingSends) {
            return -1;
        }
        m_sendQueue.push_back(std::move(request));
    }
    return resultFuture.get();
}

bool UdpChannel::isBound() const {
    return m_bound.load();
}

auto UdpChannel::localPort() const -> uint16_t {
    return m_localPort.load();
}

void UdpChannel::setReceiveCallback(
    std::function<void(std::span<const uint8_t>, const std::string&, uint16_t)> cb) {
    std::lock_guard lock(m_callbackMutex);
    m_recvCallback = std::move(cb);
}

void UdpChannel::workerLoop(std::stop_token token, UdpEndpointConfig config,
                            std::promise<std::expected<void, std::string>> initPromise) {
    QUdpSocket socket;
    if (!socket.bind(bindAddress(config), config.localPort)) {
        initPromise.set_value(
            std::unexpected("Failed to bind UDP: " + socket.errorString().toStdString()));
        return;
    }

    m_localPort.store(socket.localPort());
    m_bound.store(true);
    m_workerSocket.store(&socket);
    CurrentUdpChannel = this;
    initPromise.set_value({});

    while (true) {
        processPendingSends(socket);
        if (token.stop_requested()) {
            std::lock_guard lock(m_sendMutex);
            if (m_sendQueue.empty()) {
                break;
            }
        }

        if (socket.waitForReadyRead(static_cast<int>(UdpPollInterval.count()))) {
            onReadyRead(socket);
        }
    }

    socket.close();
    CurrentUdpChannel = nullptr;
    m_workerSocket.store(nullptr);
    m_bound.store(false);
    m_localPort.store(0);
}

void UdpChannel::processPendingSends(QUdpSocket& socket) {
    while (true) {
        std::optional<SendRequest> request;
        {
            std::lock_guard lock(m_sendMutex);
            if (m_sendQueue.empty()) {
                break;
            }
            request = std::move(m_sendQueue.front());
            m_sendQueue.pop_front();
        }

        const auto sent = sendDatagram(socket, request->data, request->host, request->port);
        request->result.set_value(sent);
    }
}

void UdpChannel::onReadyRead(QUdpSocket& socket) {
    while (socket.hasPendingDatagrams()) {
        const auto datagram = socket.receiveDatagram();
        if (!datagram.isValid()) {
            continue;
        }

        decltype(m_recvCallback) callback;
        {
            std::lock_guard lock(m_callbackMutex);
            callback = m_recvCallback;
        }
        if (!callback) {
            continue;
        }

        const auto bytes = datagram.data();
        const std::vector<uint8_t> payload(
            reinterpret_cast<const uint8_t*>(bytes.constData()),
            reinterpret_cast<const uint8_t*>(bytes.constData()) + bytes.size());
        const auto sender = datagram.senderAddress().toString().toStdString();
        const auto port = static_cast<uint16_t>(datagram.senderPort());
        callback(payload, sender, port);
    }
}

void UdpChannel::failPendingSends() {
    std::deque<SendRequest> pending;
    {
        std::lock_guard lock(m_sendMutex);
        pending.swap(m_sendQueue);
    }
    for (auto& request : pending) {
        request.result.set_value(-1);
    }
}

}  // namespace Dss::Network
