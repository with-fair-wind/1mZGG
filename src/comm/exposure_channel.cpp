#include "dss/comm/exposure_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm
{

ExposureChannel::ExposureChannel(MessageBus& bus)
    : SerialWorkerBase(bus)
{
}

auto ExposureChannel::latestData() const -> Dss::Core::ExposureDisplayData
{
    std::lock_guard lock(m_dataMutex);
    return m_latestData;
}

void ExposureChannel::decodeFrame(std::span<const uint8_t> /*data*/)
{
    // TODO: decode 23-byte exposure frame
    // Original: BCD time + pointing AE + atmosphere merge
    Dss::Core::ExposureDisplayData decoded{};
    // ... decode from raw bytes ...

    {
        std::lock_guard lock(m_dataMutex);
        m_latestData = decoded;
    }

    bus().emit(Dss::Core::ExposureSyncEvent{decoded});
}

void ExposureChannel::encodeFrame(std::span<uint8_t> buffer)
{
    // TODO: encode trigger mode, frame-rate, exposure delay
    // Original: 8-byte frame with trigger mode and 3-byte delay
    (void)buffer;
}

} // namespace Dss::Comm
