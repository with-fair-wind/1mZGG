#include "dss/comm/exposure_channel.h"

#include "dss/core/events.h"

namespace Dss::Comm {

ExposureChannel::ExposureChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

auto ExposureChannel::latestData() const -> Dss::Core::ExposureDisplayData {
    std::lock_guard lock(m_dataMutex);
    return m_latestData;
}

void ExposureChannel::sendExposureCommand(const ExposureCommand& command) {
    {
        std::lock_guard lock(m_commandMutex);
        m_pendingCommand = command;
    }
    requestSend();
}

void ExposureChannel::decodeFrame(std::span<const uint8_t> data) {
    auto decoded = decodeExposureFrameDetailed(data);
    if (!decoded) {
        const auto& error = decoded.error();
        publishDecodeError(error.field, error.message, error.byteOffset, error.rawValue);
        return;
    }

    {
        std::lock_guard lock(m_dataMutex);
        m_latestData = *decoded;
    }

    bus().emit(Dss::Core::ExposureSyncEvent{*decoded});
}

void ExposureChannel::encodeFrame(std::span<uint8_t> buffer) {
    std::lock_guard lock(m_commandMutex);
    (void)encodeExposureCommand(m_pendingCommand, buffer);
}

}  // namespace Dss::Comm
