#pragma once

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"

namespace Dss::Comm {

class MasterControlChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::MasterControl;

    explicit MasterControlChannel(MessageBus& bus);

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }
    void decodeFrame(std::span<const uint8_t> data) override;
    void encodeFrame(std::span<uint8_t> buffer) override;
};

}  // namespace Dss::Comm
