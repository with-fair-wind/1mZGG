#include "dss/comm/master_control_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm
{

MasterControlChannel::MasterControlChannel(MessageBus& bus)
    : SerialWorkerBase(bus)
{
}

void MasterControlChannel::decodeFrame(std::span<const uint8_t> data)
{
    // TODO: decode 30-byte master control frame
    // Original: exposure, track mode, task IDs, time window
    Dss::Core::MasterControlEvent event{};
    // ... decode from raw bytes ...
    (void)data;

    bus().emit(event);
}

void MasterControlChannel::encodeFrame(std::span<uint8_t> buffer)
{
    // TODO: encode 28-byte response frame with tracking state
    (void)buffer;
}

} // namespace Dss::Comm
