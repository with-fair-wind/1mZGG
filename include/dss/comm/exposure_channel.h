#pragma once

#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

#include <mutex>

namespace Dss::Comm
{

class ExposureChannel final : public SerialWorkerBase
{
public:
    static constexpr size_t RECV_SIZE = 23;
    static constexpr size_t SEND_SIZE = 8;

    explicit ExposureChannel(MessageBus& bus);

    [[nodiscard]] auto latestData() const -> Dss::Core::ExposureDisplayData;

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override { return RECV_SIZE; }
    [[nodiscard]] auto sendFrameSize() const -> size_t override { return SEND_SIZE; }
    void decodeFrame(std::span<const uint8_t> data) override;
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    mutable std::mutex m_dataMutex;
    Dss::Core::ExposureDisplayData m_latestData{};
};

} // namespace Dss::Comm
