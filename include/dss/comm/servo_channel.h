#pragma once

#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

#include <mutex>

namespace Dss::Comm
{

class ServoChannel final : public SerialWorkerBase
{
public:
    static constexpr size_t RECV_SIZE = 20;
    static constexpr size_t SEND_SIZE = 14;

    explicit ServoChannel(MessageBus& bus);

    void setTrackResult(const Dss::Core::TargetInfo& target);

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override { return RECV_SIZE; }
    [[nodiscard]] auto sendFrameSize() const -> size_t override { return SEND_SIZE; }
    void decodeFrame(std::span<const uint8_t> data) override;
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    std::mutex m_targetMutex;
    Dss::Core::TargetInfo m_currentTarget{};
};

} // namespace Dss::Comm
