#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "dss/comm/frame_codec.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/core/constants.h"
#include "dss/core/event_bus.h"

class QSerialPort;

namespace Dss::Comm {

class SerialWorkerBase : public ISerialChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit SerialWorkerBase(MessageBus& bus);
    ~SerialWorkerBase() override;

    SerialWorkerBase(const SerialWorkerBase&) = delete;
    SerialWorkerBase& operator=(const SerialWorkerBase&) = delete;

    auto open(const SerialConfig& config) -> std::expected<void, std::string> override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    [[nodiscard]] int recvFramesPerSec() const {
        return m_recvFps.load();
    }
    [[nodiscard]] int sendFramesPerSec() const {
        return m_sendFps.load();
    }

protected:
    [[nodiscard]] virtual auto recvFrameSize() const -> size_t override = 0;
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t override = 0;

    virtual void decodeFrame(std::span<const uint8_t> data) = 0;
    virtual void encodeFrame(std::span<uint8_t> buffer) = 0;

    void requestSend();

    MessageBus& bus() {
        return m_bus;
    }

private:
    void workerLoop(std::stop_token token);
    void onDataReceived();
    void sendFrameInternal();

    MessageBus& m_bus;
    SerialConfig m_config{};
    std::unique_ptr<QSerialPort> m_serialPort;
    std::jthread m_workerThread;

    std::mutex m_sendMutex;
    bool m_sendRequested = false;

    std::atomic<Dss::Core::Status> m_status{Dss::Core::Status::Init};
    std::atomic<int> m_recvFps{0};
    std::atomic<int> m_sendFps{0};
    std::atomic<int> m_recvCount{0};
    std::atomic<int> m_sendCount{0};
};

}  // namespace Dss::Comm
