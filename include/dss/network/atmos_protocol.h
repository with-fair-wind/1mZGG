#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Dss::Network {

inline constexpr size_t AtmosPacketSize = sizeof(int32_t) + 3U * sizeof(double);

struct AtmosSample {
    int32_t frameHead = 0;
    double temperature = 0.0;
    double humidity = 0.0;
    double pressure = 0.0;
};

namespace Detail {

[[nodiscard]] inline auto readI32Le(std::span<const uint8_t> data, size_t offset) -> int32_t {
    const auto raw = static_cast<uint32_t>(data[offset]) |
                     (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
                     (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
                     (static_cast<uint32_t>(data[offset + 3U]) << 24U);
    return std::bit_cast<int32_t>(raw);
}

[[nodiscard]] inline auto readDoubleLe(std::span<const uint8_t> data, size_t offset) -> double {
    uint64_t raw = 0;
    for (size_t i = 0; i < sizeof(double); ++i) {
        raw |= static_cast<uint64_t>(data[offset + i]) << (i * 8U);
    }
    return std::bit_cast<double>(raw);
}

}  // namespace Detail

[[nodiscard]] inline auto decodeAtmosPacket(std::span<const uint8_t> data)
    -> std::optional<AtmosSample> {
    if (data.size() < AtmosPacketSize) {
        return std::nullopt;
    }

    constexpr size_t temperatureOffset = sizeof(int32_t);
    constexpr size_t humidityOffset = temperatureOffset + sizeof(double);
    constexpr size_t pressureOffset = humidityOffset + sizeof(double);

    return AtmosSample{
        .frameHead = Detail::readI32Le(data, 0),
        .temperature = Detail::readDoubleLe(data, temperatureOffset),
        .humidity = Detail::readDoubleLe(data, humidityOffset),
        .pressure = Detail::readDoubleLe(data, pressureOffset) * 100.0,
    };
}

}  // namespace Dss::Network
