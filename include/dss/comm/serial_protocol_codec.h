#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "dss/comm/frame_codec.h"
#include "dss/core/events.h"
#include "dss/core/types.h"

namespace Dss::Comm {

enum class SerialProtocol {
    Display,
    Exposure,
    MasterControl,
    Servo,
};

struct SerialFrameLayout {
    SerialProtocol protocol;
    std::string_view name;
    std::size_t recvSize = 0;
    std::size_t sendSize = 0;
};

inline constexpr std::array<SerialFrameLayout, 4> SerialFrameLayouts{
    SerialFrameLayout{SerialProtocol::Display, "display", 20, 9},
    SerialFrameLayout{SerialProtocol::Exposure, "exposure", 23, 8},
    SerialFrameLayout{SerialProtocol::MasterControl, "master_control", 30, 28},
    SerialFrameLayout{SerialProtocol::Servo, "servo", 20, 14},
};

[[nodiscard]] constexpr auto layoutFor(SerialProtocol protocol) -> SerialFrameLayout {
    for (const auto& layout : SerialFrameLayouts) {
        if (layout.protocol == protocol) {
            return layout;
        }
    }
    return {};
}

[[nodiscard]] inline bool validateReceiveFrame(SerialProtocol protocol,
                                               std::span<const uint8_t> frame) {
    return FrameCodec::validate(frame, layoutFor(protocol).recvSize);
}

inline bool prepareSendFrame(SerialProtocol protocol, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(protocol).sendSize) {
        return false;
    }
    FrameCodec::wrap(frame);
    return true;
}

enum class ExposureTriggerMode {
    External,
    FreeRun,
};

struct ExposureCommand {
    ExposureTriggerMode triggerMode = ExposureTriggerMode::External;
    uint8_t frameFrequencyCode = 0x01;
    uint32_t exposureDelayTicks = 0;
};

struct MasterControlCommand {
    float exposure = 0.0F;
    uint8_t mode1 = 0;
    uint8_t mode2 = 0;
    int trackMode = -1;
    bool track = false;
    bool save = false;
    bool grab = false;
    uint32_t targetId = 0;
    uint32_t taskId = 0;
    Dss::Core::TimeOfDay start{};
    Dss::Core::TimeOfDay end{};
};

struct ServoCorrection {
    bool distanceValid = false;
    bool speedValid = false;
    Dss::Core::Vec2f distanceArcsec{};
    Dss::Core::Vec2f speedArcsecPerSec{};
    uint8_t mode = 0x19;
};

struct MasterControlStatus {
    ServoCorrection correction{};
    Dss::Core::Timestamp timestamp{};
    Dss::Core::Vec2f pointingAe{};
};

namespace detail {

inline constexpr double AngleCodeDenominator = static_cast<double>(1ULL << 29U);

[[nodiscard]] inline auto invalidFrameError(SerialProtocol protocol) -> std::string {
    return std::string("invalid ") + std::string(layoutFor(protocol).name) + " frame";
}

[[nodiscard]] constexpr auto readU16Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint16_t {
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1U]) << 8U);
}

[[nodiscard]] constexpr auto readU24Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint32_t {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2U]) << 16U);
}

[[nodiscard]] constexpr auto readU32Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint32_t {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<uint32_t>(data[offset + 3U]) << 24U);
}

constexpr void writeU16Le(std::span<uint8_t> data, std::size_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

constexpr void writeU24Le(std::span<uint8_t> data, std::size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
}

constexpr void writeU32Le(std::span<uint8_t> data, std::size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] constexpr auto decodeBcd(uint8_t value) -> int {
    return static_cast<int>(((value >> 4U) & 0x0FU) * 10U + (value & 0x0FU));
}

[[nodiscard]] constexpr auto encodeBcd(int value) -> uint8_t {
    const auto clamped = std::clamp(value, 0, 99);
    return static_cast<uint8_t>(((clamped / 10) << 4U) | (clamped % 10));
}

[[nodiscard]] inline auto decodeAngle(uint32_t code) -> float {
    return static_cast<float>(static_cast<double>(code) / AngleCodeDenominator * 360.0);
}

[[nodiscard]] inline auto encodeAngle(float degrees) -> uint32_t {
    const auto normalized = std::clamp(static_cast<double>(degrees), 0.0, 360.0);
    return static_cast<uint32_t>(std::llround(normalized / 360.0 * AngleCodeDenominator));
}

[[nodiscard]] inline auto decodePointingFrame(std::span<const uint8_t> frame,
                                              SerialProtocol protocol)
    -> std::expected<Dss::Core::ExposureDisplayData, std::string> {
    if (!validateReceiveFrame(protocol, frame)) {
        return std::unexpected(invalidFrameError(protocol));
    }

    Dss::Core::ExposureDisplayData decoded{};
    decoded.timestamp.year = 2000 + decodeBcd(frame[1]);
    decoded.timestamp.month = decodeBcd(frame[2]);
    decoded.timestamp.day = decodeBcd(frame[3]);
    decoded.timestamp.hour = decodeBcd(frame[4]);
    decoded.timestamp.minute = decodeBcd(frame[5]);
    decoded.timestamp.second = decodeBcd(frame[6]);
    decoded.timestamp.millisecond = static_cast<int>(readU16Le(frame, 7U) / 10U);
    decoded.pointingAe.x = decodeAngle(readU32Le(frame, 9U));
    decoded.pointingAe.y = decodeAngle(readU32Le(frame, 13U));
    return decoded;
}

inline void encodeSignedMagnitude16(std::span<uint8_t> frame, std::size_t offset, float value,
                                    float scale) {
    const auto magnitude = static_cast<uint16_t>(
        std::min<uint32_t>(static_cast<uint32_t>(std::lround(std::fabs(value) * scale)), 0x7FFFU));
    frame[offset] = static_cast<uint8_t>(magnitude & 0xFFU);
    frame[offset + 1U] = static_cast<uint8_t>((magnitude >> 8U) & 0x7FU);
    if (value < 0.0F) {
        frame[offset + 1U] = static_cast<uint8_t>(frame[offset + 1U] | 0x80U);
    }
}

inline void encodeSignedMagnitude24(std::span<uint8_t> frame, std::size_t offset, float value) {
    const auto magnitude =
        std::min<uint32_t>(static_cast<uint32_t>(std::lround(std::fabs(value))), 0x7FFFFFU);
    frame[offset] = static_cast<uint8_t>(magnitude & 0xFFU);
    frame[offset + 1U] = static_cast<uint8_t>((magnitude >> 8U) & 0xFFU);
    frame[offset + 2U] = static_cast<uint8_t>((magnitude >> 16U) & 0x7FU);
    if (value < 0.0F) {
        frame[offset + 2U] = static_cast<uint8_t>(frame[offset + 2U] | 0x80U);
    }
}

}  // namespace detail

[[nodiscard]] inline auto decodeDisplayFrame(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, std::string> {
    return detail::decodePointingFrame(frame, SerialProtocol::Display);
}

[[nodiscard]] inline auto decodeExposureFrame(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, std::string> {
    return detail::decodePointingFrame(frame, SerialProtocol::Exposure);
}

inline bool encodeExposureCommand(const ExposureCommand& command, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(SerialProtocol::Exposure).sendSize) {
        return false;
    }

    std::ranges::fill(frame, uint8_t{0});
    FrameCodec::wrap(frame);
    frame[1] = command.triggerMode == ExposureTriggerMode::External ? 0x00U : 0x01U;
    frame[2] = command.frameFrequencyCode;
    detail::writeU24Le(frame, 3U, command.exposureDelayTicks);
    return true;
}

[[nodiscard]] constexpr auto trackModeFromLegacyModes(uint8_t mode1, uint8_t mode2) -> int {
    switch (mode1) {
        case 0x01:
        case 0x04:
            return mode2 == 0x00 ? 0 : 1;
        case 0x02:
            return 0;
        case 0x03:
            return 2;
        case 0x05:
            return 3;
        default:
            return -1;
    }
}

[[nodiscard]] inline auto decodeMasterControlFrame(std::span<const uint8_t> frame)
    -> std::expected<MasterControlCommand, std::string> {
    if (!validateReceiveFrame(SerialProtocol::MasterControl, frame)) {
        return std::unexpected(detail::invalidFrameError(SerialProtocol::MasterControl));
    }

    MasterControlCommand command{};
    command.exposure = static_cast<float>(detail::readU24Le(frame, 3U)) / 100.0F;
    command.mode1 = frame[7];
    command.mode2 = frame[8];
    command.trackMode = trackModeFromLegacyModes(command.mode1, command.mode2);
    command.track = frame[9] == 0xFFU;
    command.grab = command.track;
    command.save = frame[10] == 0xFFU;
    command.targetId = detail::readU24Le(frame, 11U);
    command.taskId = detail::readU24Le(frame, 14U);
    command.start = Dss::Core::TimeOfDay{frame[17], frame[18], frame[19]};
    command.end = Dss::Core::TimeOfDay{frame[20], frame[21], frame[22]};
    return command;
}

[[nodiscard]] inline auto toMasterControlEvent(const MasterControlCommand& command)
    -> Dss::Core::MasterControlEvent {
    return Dss::Core::MasterControlEvent{
        .exposure = command.exposure,
        .trackMode = command.trackMode,
        .mode1 = command.mode1,
        .mode2 = command.mode2,
        .save = command.save,
        .grab = command.grab,
        .track = command.track,
        .targetId = command.targetId,
        .taskId = command.taskId,
        .start = command.start,
        .end = command.end,
    };
}

inline bool encodeServoCorrection(const ServoCorrection& correction, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(SerialProtocol::Servo).sendSize) {
        return false;
    }

    std::ranges::fill(frame, uint8_t{0});
    FrameCodec::wrap(frame);
    frame[1] = correction.distanceValid ? 0x01U : 0x00U;
    if (correction.speedValid) {
        frame[1] = static_cast<uint8_t>(frame[1] | 0x10U);
    }
    detail::encodeSignedMagnitude16(frame, 2U, correction.distanceArcsec.x, 10.0F);
    detail::encodeSignedMagnitude16(frame, 4U, correction.distanceArcsec.y, 10.0F);
    detail::encodeSignedMagnitude24(frame, 6U, correction.speedArcsecPerSec.x);
    detail::encodeSignedMagnitude24(frame, 9U, correction.speedArcsecPerSec.y);
    frame[12] = correction.mode;
    return true;
}

inline bool encodeMasterControlStatus(const MasterControlStatus& status, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(SerialProtocol::MasterControl).sendSize) {
        return false;
    }

    std::ranges::fill(frame, uint8_t{0});
    FrameCodec::wrap(frame);
    frame[1] = status.correction.distanceValid ? 0x01U : 0x00U;
    if (status.correction.speedValid) {
        frame[1] = static_cast<uint8_t>(frame[1] | 0x10U);
    }
    detail::encodeSignedMagnitude16(frame, 2U, status.correction.distanceArcsec.x, 10.0F);
    detail::encodeSignedMagnitude16(frame, 4U, status.correction.distanceArcsec.y, 10.0F);
    frame[6] = detail::encodeBcd(status.timestamp.year - 2000);
    frame[7] = detail::encodeBcd(status.timestamp.month);
    frame[8] = detail::encodeBcd(status.timestamp.day);
    frame[9] = detail::encodeBcd(status.timestamp.hour);
    frame[10] = detail::encodeBcd(status.timestamp.minute);
    frame[11] = detail::encodeBcd(status.timestamp.second);
    detail::writeU16Le(
        frame, 12U,
        static_cast<uint16_t>(std::clamp(status.timestamp.millisecond * 10, 0, 0xFFFF)));
    detail::writeU32Le(frame, 14U, detail::encodeAngle(status.pointingAe.x));
    detail::writeU32Le(frame, 18U, detail::encodeAngle(status.pointingAe.y));
    return true;
}

}  // namespace Dss::Comm
