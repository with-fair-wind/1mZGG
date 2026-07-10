#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Dss::Network {

inline constexpr size_t AtmosPacketSize =
    sizeof(int32_t) + 3U * sizeof(double);  ///< 气象报文固定字节长度

/// 解码后的气象采样数据
struct AtmosSample {
    int32_t frameHead = 0;     ///< 帧头标识
    double temperature = 0.0;  ///< 温度（摄氏度）
    double humidity = 0.0;     ///< 相对湿度
    double pressure = 0.0;     ///< 大气压（已换算为百帕）
};

namespace Detail {

/**
 * @brief 从小端序字节流读取 32 位有符号整数
 * @param data 原始字节数据
 * @param offset 起始偏移量
 * @return 解析后的整数值
 */
[[nodiscard]] inline auto readI32Le(std::span<const uint8_t> data, size_t offset) -> int32_t {
    const auto raw = static_cast<uint32_t>(data[offset]) |
                     (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
                     (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
                     (static_cast<uint32_t>(data[offset + 3U]) << 24U);
    return std::bit_cast<int32_t>(raw);
}

/**
 * @brief 从小端序字节流读取双精度浮点数
 * @param data 原始字节数据
 * @param offset 起始偏移量
 * @return 解析后的浮点值
 */
[[nodiscard]] inline auto readDoubleLe(std::span<const uint8_t> data, size_t offset) -> double {
    uint64_t raw = 0;
    for (size_t i = 0; i < sizeof(double); ++i) {
        raw |= static_cast<uint64_t>(data[offset + i]) << (i * 8U);
    }
    return std::bit_cast<double>(raw);
}

}  // namespace Detail

/**
 * @brief 解码气象 UDP 报文
 * @param data 原始字节数据
 * @return 解析成功返回采样结构，长度不足或格式无效返回空
 */
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
