#include "dss/comm/servo_channel.h"

namespace Dss::Comm {

ServoChannel::ServoChannel(MessageBus& bus) : SerialWorkerBase(bus) {}

void ServoChannel::setTrackResult(const Dss::Core::TargetInfo& target) {
    ServoCorrection correction{};
    correction.distanceValid = target.living && target.validity >= 0.8F;
    correction.speedValid = correction.distanceValid;
    if (correction.distanceValid) {
        correction.distanceArcsec = target.lastRmDm;
        correction.speedArcsecPerSec = Dss::Core::Vec2f{
            target.predictedSpdAe.x * 3600.0F,
            target.predictedSpdAe.y * 3600.0F,
        };
    }

    sendServoCorrection(correction);
}

void ServoChannel::sendServoCorrection(const ServoCorrection& correction) {
    {
        std::lock_guard lock(m_correctionMutex);
        m_pendingCorrection = correction;
    }
    requestSend();
}

void ServoChannel::decodeFrame(std::span<const uint8_t> /*data*/) {}

void ServoChannel::encodeFrame(std::span<uint8_t> buffer) {
    std::lock_guard lock(m_correctionMutex);
    (void)encodeServoCorrection(m_pendingCorrection, buffer);
}

}  // namespace Dss::Comm
