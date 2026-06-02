#pragma once

#include <mutex>

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

class ExposureChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::Exposure;

    explicit ExposureChannel(MessageBus& bus);

    [[nodiscard]] auto latestData() const -> Dss::Core::ExposureDisplayData;

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
    mutable std::mutex m_dataMutex;
    Dss::Core::ExposureDisplayData m_latestData{};
};

}  // namespace Dss::Comm
