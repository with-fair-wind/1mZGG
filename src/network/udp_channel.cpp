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

UdpChannel::UdpChannel(std::size_t maxPendingRequests, std::size_t maxPendingBytes)
    : m_maxPendingRequests(maxPendingRequests), m_maxPendingBytes(maxPendingBytes) {}

auto UdpChannel::sendQueueSnapshot() const -> SendQueueSnapshot {
    std::lock_guard lock(m_sendMutex);
    return {m_sendQueue.size(), m_pendingBytes, m_rejectedRequests, m_acceptingSends};
}

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
        m_sendWake.notify_one();
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
    const bool direct = CurrentUdpChannel == this;
    SendRequest request;
    auto resultFuture = request.result.get_future();
    {
        std::lock_guard lock(m_sendMutex);
        if (!m_acceptingSends || data.size() > maxDatagramBytes || host.size() > 256 ||
            (!direct && (m_sendQueue.size() >= m_maxPendingRequests ||
                         data.size() > m_maxPendingBytes - m_pendingBytes))) {
            ++m_rejectedRequests;
            return -1;
        }
        if (!direct) {
            // 先检查预算再复制，拒绝请求不额外保留载荷。
            request.data.assign(data.begin(), data.end());
            request.host = host;
            request.port = port;
            m_sendQueue.push_back(std::move(request));
            m_pendingBytes += data.size();
        }
    }
    if (direct) {
        return sendDatagram(*m_workerSocket.load(), data, host, port);
    }
    m_sendWake.notify_one();
    try {
        return resultFuture.get();
    } catch (const std::future_error&) {
        return -1;  // 工作线程异常时，已取出的请求也必须让等待者退出。
    }
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
    bool initialized = false;
    try {
        if (!socket.bind(bindAddress(config), config.localPort)) {
            initPromise.set_value(
                std::unexpected("Failed to bind UDP: " + socket.errorString().toStdString()));
            return;
        }
        m_localPort.store(socket.localPort());
        m_bound.store(true);
        m_workerSocket.store(&socket);
        CurrentUdpChannel = this;
        {
            std::lock_guard lock(m_sendMutex);
            m_acceptingSends = true;
        }
        initialized = true;
        initPromise.set_value({});

        while (true) {
            processPendingSends(socket);
            if (!token.stop_requested()) {
                onReadyRead(socket, token);
                if (socket.hasPendingDatagrams()) {
                    continue;  // 收发分批交替，积压接收不额外等待轮询周期。
                }
            }
            std::unique_lock lock(m_sendMutex);
            if (token.stop_requested() && m_sendQueue.empty()) {
                break;
            }
            // 发送/关闭直接唤醒；未收到命令时才按接收轮询周期等待。
            m_sendWake.wait_for(lock, UdpPollInterval, [&] {
                return token.stop_requested() || !m_acceptingSends || !m_sendQueue.empty();
            });
        }
    } catch (...) {
        if (!initialized) {
            initPromise.set_value(std::unexpected("UDP worker initialization failed"));
        }
        // 回调或分配异常不能逃出线程入口，未发送请求统一失败。
    }
    {
        std::lock_guard lock(m_sendMutex);
        m_acceptingSends = false;
    }
    failPendingSends();
    socket.close();
    CurrentUdpChannel = nullptr;
    m_workerSocket.store(nullptr);
    m_bound.store(false);
    m_localPort.store(0);
}

void UdpChannel::processPendingSends(QUdpSocket& socket) {
    for (std::size_t count = 0; count < 64; ++count) {
        std::optional<SendRequest> request;
        {
            std::lock_guard lock(m_sendMutex);
            if (m_sendQueue.empty()) {
                break;
            }
            request = std::move(m_sendQueue.front());
            m_sendQueue.pop_front();
            m_pendingBytes -= request->data.size();
        }

        const auto sent = sendDatagram(socket, request->data, request->host, request->port);
        request->result.set_value(sent);
    }
}

void UdpChannel::onReadyRead(QUdpSocket& socket, std::stop_token token) {
    for (std::size_t count = 0;
         count < 64 && !token.stop_requested() && socket.hasPendingDatagrams(); ++count) {
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
        m_pendingBytes = 0;
    }
    for (auto& request : pending) {
        request.result.set_value(-1);
    }
}

}  // namespace Dss::Network
