#include "dss/comm/channel/serial_worker_base.h"

#include <QByteArray>
#include <QIODevice>
#include <QSerialPort>
#include <QString>
#include <algorithm>
#include <chrono>

#include "dss/core/event/events.h"

namespace Dss::Comm {

SerialWorkerBase::SerialWorkerBase(MessageBus& bus) : m_bus(bus) {}

SerialWorkerBase::~SerialWorkerBase() {
    close();
}

auto SerialWorkerBase::open(const SerialConfig& config) -> std::expected<void, std::string> {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
    m_config = config;
    std::promise<std::expected<void, std::string>> initPromise;
    auto initFuture = initPromise.get_future();
    m_workerThread =
        std::jthread([this, promise = std::move(initPromise)](std::stop_token token) mutable {
            workerLoop(token, std::move(promise));
        });

    auto result = initFuture.get();
    if (!result && m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    return result;
}

void SerialWorkerBase::close() {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
}

void SerialWorkerBase::closeLocked() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }

    // worker 已 join,清空旧会话残留的半帧,防重连后与新字节拼接导致首帧丢失/误解码
    m_rxAccumulator.clear();
    {
        std::lock_guard lock(m_sendMutex);
        m_sendRequested = false;
    }
    m_open.store(false);
    m_status.store(Dss::Core::Status::Init);
}

bool SerialWorkerBase::isOpen() const {
    return m_open.load();
}

auto SerialWorkerBase::status() const -> Dss::Core::Status {
    return m_status.load();
}

void SerialWorkerBase::requestSend() {
    std::lock_guard lock(m_sendMutex);
    m_sendRequested = true;
}

void SerialWorkerBase::publishDecodeError(std::string_view field, std::string_view message,
                                          std::size_t byteOffset, uint64_t rawValue) {
    m_bus.emit(Dss::Core::SerialDecodeErrorEvent{
        .channel = std::string(channelName()),
        .message = std::string(message),
        .field = std::string(field),
        .byteOffset = static_cast<uint64_t>(byteOffset),
        .rawValue = rawValue,
    });
}

void SerialWorkerBase::workerLoop(std::stop_token token,
                                  std::promise<std::expected<void, std::string>> initPromise) {
    QSerialPort serialPort;
    serialPort.setPortName(QString::fromStdString(m_config.portName));
    serialPort.setBaudRate(m_config.baudRate);
    serialPort.setDataBits(static_cast<QSerialPort::DataBits>(m_config.dataBits));
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);
    if (!serialPort.open(QIODevice::ReadWrite)) {
        m_status.store(Dss::Core::Status::Error);
        initPromise.set_value(std::unexpected("Failed to open port: " + m_config.portName + " - " +
                                              serialPort.errorString().toStdString()));
        return;
    }

    m_open.store(true);
    m_status.store(Dss::Core::Status::Ok);
    initPromise.set_value({});

    using namespace std::chrono;
    auto lastFpsTime = steady_clock::now();

    while (!token.stop_requested()) {
        if (serialPort.waitForReadyRead(20)) {
            onDataReceived(serialPort);
        }

        bool sendRequested = false;
        {
            std::lock_guard lock(m_sendMutex);
            sendRequested = m_sendRequested;
            m_sendRequested = false;
        }
        // 锁外执行发送:sendFrameInternal 可能 m_bus.emit,持锁会与 requestSend() 重入死锁
        if (sendRequested) {
            sendFrameInternal(serialPort);
        }

        auto now = steady_clock::now();
        if (duration_cast<seconds>(now - lastFpsTime).count() >= 1) {
            m_recvFps.store(m_recvCount.exchange(0));
            m_sendFps.store(m_sendCount.exchange(0));
            lastFpsTime = now;
        }
    }
    serialPort.close();
    m_open.store(false);
}

void SerialWorkerBase::onDataReceived(QSerialPort& serialPort) {
    if (serialPort.bytesAvailable() <= 0) {
        return;
    }
    const QByteArray chunk = serialPort.readAll();
    processReceivedBytes(
        {reinterpret_cast<const uint8_t*>(chunk.constData()), static_cast<std::size_t>(chunk.size())});
}

void SerialWorkerBase::processReceivedBytes(std::span<const uint8_t> bytes) {
    const auto expected = recvFrameSize();
    if (expected == 0U || bytes.empty()) {
        return;
    }
    m_rxAccumulator.insert(m_rxAccumulator.end(), bytes.begin(), bytes.end());
    drainBufferedFrames(expected);
}

void SerialWorkerBase::drainBufferedFrames(std::size_t expected) {
    if (expected == 0U) {
        return;
    }
    while (m_rxAccumulator.size() >= expected) {
        // 首字节非帧头:向前扫描到下一个帧头,丢弃失步前缀(静默对齐,避免错误风暴)
        if (m_rxAccumulator.front() != FrameCodec::HEADER) {
            const auto nextHeader = std::ranges::find(m_rxAccumulator, FrameCodec::HEADER);
            if (nextHeader == m_rxAccumulator.end()) {
                m_rxAccumulator.clear();
                break;
            }
            m_rxAccumulator.erase(m_rxAccumulator.begin(), nextHeader);
            if (m_rxAccumulator.size() < expected) {
                break;
            }
        }

        const std::span<const uint8_t> frame(m_rxAccumulator.data(), expected);
        const auto validation = FrameCodec::validateDetailed(frame, expected);
        if (validation.valid) {
            decodeFrame(frame);
            m_recvCount.fetch_add(1);
            m_rxAccumulator.erase(
                m_rxAccumulator.begin(),
                m_rxAccumulator.begin() +
                    static_cast<std::vector<std::uint8_t>::difference_type>(expected));
        } else {
            m_bus.emit(Dss::Core::SerialFrameErrorEvent{
                .channel = std::string(channelName()),
                .message = std::string(FrameCodec::failureMessage(validation.failure)),
                .expectedBytes = static_cast<uint64_t>(validation.expectedSize),
                .actualBytes = static_cast<uint64_t>(validation.actualSize),
                .observedHeader = validation.observedHeader,
                .observedTail = validation.observedTail,
            });
            // 帧头已对齐但校验仍失败:丢首字节逐步滑窗,直至重新对齐
            m_rxAccumulator.erase(m_rxAccumulator.begin());
        }
    }
}

void SerialWorkerBase::sendFrameInternal(QSerialPort& serialPort) {
    const auto frameSize = sendFrameSize();
    std::vector<uint8_t> buffer(frameSize, 0);
    encodeFrame(buffer);
    FrameCodec::wrap(buffer);

    if (!serialPort.isOpen()) {
        return;
    }

    // 循环写完整帧:部分写时续写剩余字节,避免截断帧破坏对端帧对齐(触发 A3 类失步)
    const char* cursor = reinterpret_cast<const char*>(buffer.data());
    qint64 remaining = static_cast<qint64>(buffer.size());
    while (remaining > 0) {
        const auto written = serialPort.write(cursor, remaining);
        if (written <= 0) {
            m_bus.emit(Dss::Core::SerialFrameErrorEvent{
                .channel = std::string(channelName()),
                .message = "serial write failed: " +
                           (written < 0 ? serialPort.errorString().toStdString()
                                        : std::string("wrote zero bytes")),
                .expectedBytes = static_cast<uint64_t>(buffer.size()),
                .actualBytes = static_cast<uint64_t>(buffer.size() - remaining),
                .observedHeader = buffer.front(),
                .observedTail = buffer.back(),
            });
            return;
        }
        cursor += written;
        remaining -= written;
    }
    serialPort.flush();  // flush 仅尽量刷出,返回值不作为传输成败判据
    m_sendCount.fetch_add(1);
}

}  // namespace Dss::Comm
