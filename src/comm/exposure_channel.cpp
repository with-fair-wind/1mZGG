#include "dss/comm/exposure_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

ExposureChannel::ExposureChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

auto ExposureChannel::latestData() const -> Dss::Core::ExposureDisplayData {
    std::lock_guard lock(m_dataMutex);
    return m_latestData;
}

void ExposureChannel::decodeFrame(std::span<const uint8_t> data) {
    auto decoded = decodeExposureFrame(data);
    if (!decoded) {
        return;
    }

    {
        std::lock_guard lock(m_dataMutex);
        m_latestData = *decoded;
    }

    bus().emit(Dss::Core::ExposureSyncEvent{*decoded});
}

void ExposureChannel::encodeFrame(std::span<uint8_t> buffer) {
    (void)encodeExposureCommand(ExposureCommand{}, buffer);
}

}  // namespace Dss::Comm
