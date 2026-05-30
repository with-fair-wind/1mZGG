#include "dss/comm/serial_worker_base.h"

#include <QDebug>

#include <chrono>

namespace Dss::Comm
{

SerialWorkerBase::SerialWorkerBase(MessageBus& bus)
    : m_bus(bus)
{
}

SerialWorkerBase::~SerialWorkerBase()
{
    close();
}

auto SerialWorkerBase::open(const SerialConfig& config) -> std::expected<void, std::string>
{
    m_config = config;

    m_serialPort = std::make_unique<QSerialPort>();
    m_serialPort->setPortName(QString::fromStdString(m_config.portName));
    m_serialPort->setBaudRate(m_config.baudRate);
    m_serialPort->setDataBits(static_cast<QSerialPort::DataBits>(m_config.dataBits));
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite))
    {
        m_status.store(Dss::Core::Status::Error);
        return std::unexpected("Failed to open port: " + m_config.portName + " - " +
                               m_serialPort->errorString().toStdString());
    }

    m_status.store(Dss::Core::Status::Ok);

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });

    return {};
}

void SerialWorkerBase::close()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_workerThread.join();
    }

    if (m_serialPort && m_serialPort->isOpen())
    {
        m_serialPort->close();
    }
    m_serialPort.reset();
    m_status.store(Dss::Core::Status::Init);
}

bool SerialWorkerBase::isOpen() const
{
    return m_serialPort && m_serialPort->isOpen();
}

auto SerialWorkerBase::status() const -> Dss::Core::Status
{
    return m_status.load();
}

void SerialWorkerBase::requestSend()
{
    std::lock_guard lock(m_sendMutex);
    m_sendRequested = true;
}

void SerialWorkerBase::workerLoop(std::stop_token token)
{
    using namespace std::chrono;
    auto lastFpsTime = steady_clock::now();

    while (!token.stop_requested())
    {
        if (m_serialPort && m_serialPort->waitForReadyRead(20))
        {
            onDataReceived();
        }

        {
            std::lock_guard lock(m_sendMutex);
            if (m_sendRequested)
            {
                m_sendRequested = false;
                sendFrameInternal();
            }
        }

        auto now = steady_clock::now();
        if (duration_cast<seconds>(now - lastFpsTime).count() >= 1)
        {
            m_recvFps.store(m_recvCount.exchange(0));
            m_sendFps.store(m_sendCount.exchange(0));
            lastFpsTime = now;
        }
    }
}

void SerialWorkerBase::onDataReceived()
{
    const auto expected = recvFrameSize();
    while (m_serialPort->bytesAvailable() >= static_cast<qint64>(expected))
    {
        QByteArray raw = m_serialPort->read(static_cast<qint64>(expected));
        if (raw.size() == static_cast<qsizetype>(expected))
        {
            auto data = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(raw.constData()), expected);
            if (FrameCodec::validate(data, expected))
            {
                decodeFrame(data);
                m_recvCount.fetch_add(1);
            }
        }
    }
}

void SerialWorkerBase::sendFrameInternal()
{
    const auto frameSize = sendFrameSize();
    std::vector<uint8_t> buffer(frameSize, 0);
    FrameCodec::wrap(buffer);
    encodeFrame(buffer);

    if (m_serialPort && m_serialPort->isOpen())
    {
        m_serialPort->write(reinterpret_cast<const char*>(buffer.data()), static_cast<qint64>(buffer.size()));
        m_serialPort->flush();
        m_sendCount.fetch_add(1);
    }
}

} // namespace Dss::Comm
