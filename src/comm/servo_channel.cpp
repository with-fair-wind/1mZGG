#include "dss/comm/servo_channel.h"

namespace Dss::Comm
{

ServoChannel::ServoChannel(MessageBus& bus)
    : SerialWorkerBase(bus)
{
}

void ServoChannel::setTrackResult(const Dss::Core::TargetInfo& target)
{
    {
        std::lock_guard lock(m_targetMutex);
        m_currentTarget = target;
    }
    requestSend();
}

void ServoChannel::decodeFrame(std::span<const uint8_t> /*data*/)
{
    // Servo receive path not used in original code
}

void ServoChannel::encodeFrame(std::span<uint8_t> buffer)
{
    // TODO: encode tracking offsets/speeds from m_currentTarget into 14-byte frame
    std::lock_guard lock(m_targetMutex);
    (void)buffer;
}

} // namespace Dss::Comm
