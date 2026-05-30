#pragma once

#include "dss/comm/serial_worker_base.h"

namespace Dss::Comm
{

class MasterControlChannel final : public SerialWorkerBase
{
public:
    static constexpr size_t RECV_SIZE = 30;
    static constexpr size_t SEND_SIZE = 28;

    explicit MasterControlChannel(MessageBus& bus);

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override { return RECV_SIZE; }
    [[nodiscard]] auto sendFrameSize() const -> size_t override { return SEND_SIZE; }
    void decodeFrame(std::span<const uint8_t> data) override;
    void encodeFrame(std::span<uint8_t> buffer) override;
};

} // namespace Dss::Comm
