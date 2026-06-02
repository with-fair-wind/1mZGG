#include "dss/comm/servo_channel.h"

namespace Dss::Comm {

ServoChannel::ServoChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void ServoChannel::setTrackResult(const Dss::Core::TargetInfo& target) {
    {
        std::lock_guard lock(m_targetMutex);
        m_currentTarget = target;
    }
    requestSend();
}

void ServoChannel::decodeFrame(std::span<const uint8_t> /*data*/) {
    // Servo receive path not used in original code
}

void ServoChannel::encodeFrame(std::span<uint8_t> buffer) {
    std::lock_guard lock(m_targetMutex);

    ServoCorrection correction{};
    correction.distanceValid = m_currentTarget.living && m_currentTarget.validity >= 0.8F;
    correction.speedValid = correction.distanceValid;
    if (correction.distanceValid) {
        correction.distanceArcsec = m_currentTarget.lastRmDm;
        correction.speedArcsecPerSec = Dss::Core::Vec2f{
            m_currentTarget.predictedSpdAe.x * 3600.0F,
            m_currentTarget.predictedSpdAe.y * 3600.0F,
        };
    }

    (void)encodeServoCorrection(correction, buffer);
}

}  // namespace Dss::Comm
