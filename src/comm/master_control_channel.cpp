#include "dss/comm/master_control_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

MasterControlChannel::MasterControlChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void MasterControlChannel::decodeFrame(std::span<const uint8_t> data) {
    auto command = decodeMasterControlFrame(data);
    if (!command) {
        return;
    }

    bus().emit(toMasterControlEvent(*command));
}

void MasterControlChannel::encodeFrame(std::span<uint8_t> buffer) {
    (void)encodeMasterControlStatus(MasterControlStatus{}, buffer);
}

}  // namespace Dss::Comm
