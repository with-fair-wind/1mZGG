#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Dss::Network {

inline constexpr size_t GxtcHeaderSize = 35;  ///< GXTC 协议包头字节数
inline constexpr size_t GxtcTargetSize = 29;  ///< GXTC 协议单个目标记录字节数
inline constexpr size_t GdclPacketSize = 34;  ///< GDCL 协议固定报文长度

/// GXTC 报文头部元数据
struct GxtcMetadata {
    int32_t jms1970Centiseconds = 0;  ///< JMS1970 时间（百分之一秒）
    uint8_t measureStatus = 0;        ///< 测量状态
    uint8_t taskMode = 0;             ///< 任务模式
    uint8_t targetStatus = 0;         ///< 目标状态
    double temperatureCelsius = 0.0;  ///< 温度（摄氏度）
    double atmospherePressure = 0.0;  ///< 大气压
    double humidityPercent = 0.0;     ///< 相对湿度（百分比）
};

/// GXTC 报文中的单个目标信息
struct GxtcTarget {
    uint8_t mainFlag = 0xFF;                    ///< 主目标标志
    int32_t targetId = 0;                       ///< 目标编号
    int32_t azimuthArcsec = 0;                  ///< 方位角（角秒）
    int32_t elevationArcsec = 0;                ///< 俯仰角（角秒）
    int32_t azimuthSpeedArcsecPerSecond = 0;    ///< 方位角速度（角秒/秒）
    int32_t elevationSpeedArcsecPerSecond = 0;  ///< 俯仰角速度（角秒/秒）
    double magnitude = 0.0;                     ///< 星等
};

/// GDCL 测量结果报文
struct GdclMeasurement {
    int32_t jms1970Centiseconds = 0;  ///< JMS1970 时间（百分之一秒）
    uint8_t measureStatus = 0;        ///< 测量状态
    int32_t targetId = 0;             ///< 目标编号
    uint8_t dataFlag = 1;             ///< 数据标志
    int32_t dn = 0;                   ///< DN 值
    int32_t magnitudeCentivalue = 0;  ///< 星等（百分之一）
    int32_t distance = 0;             ///< 距离
    int32_t alphaArcsec = 0;          ///< 赤经（角秒）
    int32_t deltaArcsec = 0;          ///< 赤纬（角秒）
    int32_t magnitudeResult = 1;      ///< 星等计算结果
};

namespace Detail {

/**
 * @brief 向缓冲区追加一个无符号 8 位整数
 * @param output 目标字节缓冲区
 * @param value 待写入的值
 */
inline void appendU8(std::vector<uint8_t>& output, uint8_t value) {
    output.push_back(value);
}

/**
 * @brief 向缓冲区追加小端序 32 位有符号整数
 * @param output 目标字节缓冲区
 * @param value 待写入的值
 */
inline void appendI32Le(std::vector<uint8_t>& output, int32_t value) {
    const auto raw = std::bit_cast<uint32_t>(value);
    output.push_back(static_cast<uint8_t>(raw & 0xFFU));
    output.push_back(static_cast<uint8_t>((raw >> 8U) & 0xFFU));
    output.push_back(static_cast<uint8_t>((raw >> 16U) & 0xFFU));
    output.push_back(static_cast<uint8_t>((raw >> 24U) & 0xFFU));
}

/**
 * @brief 向缓冲区追加小端序双精度浮点数
 * @param output 目标字节缓冲区
 * @param value 待写入的值
 */
inline void appendDoubleLe(std::vector<uint8_t>& output, double value) {
    const auto raw = std::bit_cast<uint64_t>(value);
    for (size_t i = 0; i < 8U; ++i) {
        output.push_back(static_cast<uint8_t>((raw >> (i * 8U)) & 0xFFU));
    }
}

}  // namespace Detail

/**
 * @brief 编码 GXTC 协议报文
 * @param metadata 报文头部元数据
 * @param targets 目标列表
 * @return 编码后的字节序列
 */
[[nodiscard]] inline auto buildGxtcPacket(const GxtcMetadata& metadata,
                                          std::span<const GxtcTarget> targets)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> packet;
    packet.reserve(GxtcHeaderSize + targets.size() * GxtcTargetSize);

    Detail::appendI32Le(packet, static_cast<int32_t>(targets.size()));
    Detail::appendI32Le(packet, metadata.jms1970Centiseconds);
    Detail::appendU8(packet, metadata.measureStatus);
    Detail::appendU8(packet, metadata.taskMode);
    Detail::appendU8(packet, metadata.targetStatus);
    Detail::appendDoubleLe(packet, metadata.temperatureCelsius);
    Detail::appendDoubleLe(packet, metadata.atmospherePressure);
    Detail::appendDoubleLe(packet, metadata.humidityPercent);

    for (const auto& target : targets) {
        Detail::appendU8(packet, target.mainFlag);
        Detail::appendI32Le(packet, target.targetId);
        Detail::appendI32Le(packet, target.azimuthArcsec);
        Detail::appendI32Le(packet, target.elevationArcsec);
        Detail::appendI32Le(packet, target.azimuthSpeedArcsecPerSecond);
        Detail::appendI32Le(packet, target.elevationSpeedArcsecPerSecond);
        Detail::appendDoubleLe(packet, target.magnitude);
    }

    return packet;
}

/**
 * @brief 编码 GDCL 协议报文
 * @param measurement 测量结果
 * @return 编码后的字节序列
 */
[[nodiscard]] inline auto buildGdclPacket(const GdclMeasurement& measurement)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> packet;
    packet.reserve(GdclPacketSize);

    Detail::appendI32Le(packet, measurement.jms1970Centiseconds);
    Detail::appendU8(packet, measurement.measureStatus);
    Detail::appendI32Le(packet, measurement.targetId);
    Detail::appendU8(packet, measurement.dataFlag);
    Detail::appendI32Le(packet, measurement.dn);
    Detail::appendI32Le(packet, measurement.magnitudeCentivalue);
    Detail::appendI32Le(packet, measurement.distance);
    Detail::appendI32Le(packet, measurement.alphaArcsec);
    Detail::appendI32Le(packet, measurement.deltaArcsec);
    Detail::appendI32Le(packet, measurement.magnitudeResult);

    return packet;
}

}  // namespace Dss::Network
