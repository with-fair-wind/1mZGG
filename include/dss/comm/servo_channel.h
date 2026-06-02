#pragma once

#include <mutex>

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

class ServoChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::Servo;

    explicit ServoChannel(MessageBus& bus);

    void setTrackResult(const Dss::Core::TargetInfo& target);

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }
    void decodeFrame(std::span<const uint8_t> data) override;
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    std::mutex m_targetMutex;
    Dss::Core::TargetInfo m_currentTarget{};
};

}  // namespace Dss::Comm
