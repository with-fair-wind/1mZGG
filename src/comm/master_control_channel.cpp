#include "dss/comm/master_control_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

MasterControlChannel::MasterControlChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void MasterControlChannel::sendMasterControlStatus(const MasterControlStatus& status) {
    {
        std::lock_guard lock(m_statusMutex);
        m_pendingStatus = status;
    }
    requestSend();
}

void MasterControlChannel::decodeFrame(std::span<const uint8_t> data) {
    auto command = decodeMasterControlFrameDetailed(data);
    if (!command) {
        const auto& error = command.error();
        publishDecodeError(error.field, error.message, error.byteOffset, error.rawValue);
        return;
    }

    bus().emit(toMasterControlEvent(*command));
}

void MasterControlChannel::encodeFrame(std::span<uint8_t> buffer) {
    std::lock_guard lock(m_statusMutex);
    (void)encodeMasterControlStatus(m_pendingStatus, buffer);
}

}  // namespace Dss::Comm
