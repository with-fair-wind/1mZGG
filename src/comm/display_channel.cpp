#include "dss/comm/display_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm
{

DisplayChannel::DisplayChannel(MessageBus& bus)
    : SerialWorkerBase(bus)
{
}

void DisplayChannel::decodeFrame(std::span<const uint8_t> /*data*/)
{
    // TODO: decode BCD date/time + azimuth/elevation from 20-byte frame
    // Original: bytes 1-8 = BCD time, bytes 9-16 = 29-bit AE fixed-point
    bus().emit(Dss::Core::Sync25HzEvent{});
}

void DisplayChannel::encodeFrame(std::span<uint8_t> /*buffer*/)
{
    // Display channel send path is mostly empty in original code
}

} // namespace Dss::Comm
