#include "dss/comm/display_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

DisplayChannel::DisplayChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void DisplayChannel::decodeFrame(std::span<const uint8_t> data) {
    auto decoded = decodeDisplayFrameDetailed(data);
    if (!decoded) {
        const auto& error = decoded.error();
        publishDecodeError(error.field, error.message, error.byteOffset, error.rawValue);
        return;
    }

    bus().emit(Dss::Core::Sync25HzEvent{});
}

void DisplayChannel::encodeFrame(std::span<uint8_t> /*buffer*/) {}

}  // namespace Dss::Comm
