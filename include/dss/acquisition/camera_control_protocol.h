#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Dss::Acquisition {

using CameraCommand = std::array<uint8_t, 3>;

[[nodiscard]] inline auto buildGrabCommand(bool start) -> CameraCommand {
    return start ? CameraCommand{0x51, 0x47, 0x03} : CameraCommand{0x51, 0x07, 0x03};
}

[[nodiscard]] inline auto encodeRegisterCommand(uint8_t registerBank, uint8_t address, uint8_t data)
    -> CameraCommand {
    const auto value = (static_cast<uint32_t>(data) << 8U) | static_cast<uint32_t>(address);
    return CameraCommand{
        static_cast<uint8_t>((value << 4U) | (static_cast<uint32_t>(registerBank) << 2U) | 1U),
        static_cast<uint8_t>(((value >> 4U) << 2U) | 3U),
        static_cast<uint8_t>(((value >> 10U) << 2U) | 3U),
    };
}

[[nodiscard]] inline auto buildExposureCommands(float exposureMilliseconds)
    -> std::vector<CameraCommand> {
    const auto clampedExposure = std::max(exposureMilliseconds, 1.3F);
    const auto exposureTicks = static_cast<uint32_t>(clampedExposure / 0.045F);

    constexpr uint8_t registerBank = 2;
    return {
        encodeRegisterCommand(registerBank, 0, static_cast<uint8_t>(exposureTicks & 0xFFU)),
        encodeRegisterCommand(registerBank, 1, static_cast<uint8_t>((exposureTicks >> 8U) & 0xFFU)),
        encodeRegisterCommand(registerBank, 2,
                              static_cast<uint8_t>((exposureTicks >> 16U) & 0xFFU)),
    };
}

[[nodiscard]] inline auto buildGainCommands(float /*gain*/) -> std::vector<CameraCommand> {
    constexpr uint8_t legacyGain = 14;
    constexpr uint8_t registerBank = 1;
    return {
        encodeRegisterCommand(registerBank, 0x0F, static_cast<uint8_t>((legacyGain << 4U) | 0x02U)),
        encodeRegisterCommand(registerBank, 0x10,
                              static_cast<uint8_t>(((legacyGain & 0x30U) >> 4U) | 0x30U)),
        encodeRegisterCommand(registerBank, 0x25, static_cast<uint8_t>((legacyGain & 0x3FU) << 1U)),
    };
}

[[nodiscard]] inline auto buildWindowCommands(int line, int start, int fullWidth, int subWidth)
    -> std::vector<CameraCommand> {
    constexpr uint8_t registerBank = 2;
    const auto lineValue = static_cast<uint32_t>(line);
    const auto startValue = static_cast<uint32_t>(start);
    const auto startLow = static_cast<uint8_t>(startValue & 0xFFU);
    const auto startHigh = static_cast<uint8_t>((startValue >> 8U) & 0xFFU);
    const auto lineLow = static_cast<uint8_t>(lineValue & 0xFFU);
    const auto lineHigh = static_cast<uint8_t>((lineValue >> 8U) & 0xFFU);

    if (line == fullWidth) {
        return {
            encodeRegisterCommand(registerBank, 4, startLow),
            encodeRegisterCommand(registerBank, 5, startHigh),
            encodeRegisterCommand(registerBank, 6, lineLow),
            encodeRegisterCommand(registerBank, 7, lineHigh),
        };
    }

    if (line == subWidth) {
        return {
            encodeRegisterCommand(registerBank, 6, lineLow),
            encodeRegisterCommand(registerBank, 7, lineHigh),
            encodeRegisterCommand(registerBank, 4, startLow),
            encodeRegisterCommand(registerBank, 5, startHigh),
        };
    }

    return {};
}

[[nodiscard]] inline auto decodeTemperature(std::span<const uint8_t> data) -> std::optional<float> {
    if (data.size() < 3U) {
        return std::nullopt;
    }

    const auto value = static_cast<uint32_t>((((data[0] & 0x7FU) << 8U) | (data[1] & 0xF8U)) >> 3U);
    if (data[0] < 128U) {
        return static_cast<float>(value) / 10.0F - 75.0F;
    }
    return (static_cast<float>(value) - 4096.0F) / 10.0F - 75.0F;
}

}  // namespace Dss::Acquisition
