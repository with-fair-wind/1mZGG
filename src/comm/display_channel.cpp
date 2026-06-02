#include "dss/comm/display_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

DisplayChannel::DisplayChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void DisplayChannel::decodeFrame(std::span<const uint8_t> data) {
    if (decodeDisplayFrame(data)) {
        bus().emit(Dss::Core::Sync25HzEvent{});
    }
}

void DisplayChannel::encodeFrame(std::span<uint8_t> /*buffer*/) {
    // Display channel send path is mostly empty in original code
}

}  // namespace Dss::Comm
