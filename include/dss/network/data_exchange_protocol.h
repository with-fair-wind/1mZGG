#pragma once

#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "dss/core/types.h"

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

/// 数据交换协议映射选项。
struct DataExchangeMappingOptions {
    int32_t jms1970Centiseconds = 0;  ///< JMS1970 时间（百分之一秒）
    uint8_t measureStatus = 0;        ///< 测量状态
    uint8_t taskMode = 2;             ///< GXTC 任务模式
    uint8_t validTargetStatus = 1;    ///< 有效目标状态码
    uint8_t invalidTargetStatus = 2;  ///< 无效目标状态码
    uint8_t mainFlag = 0xFF;          ///< GXTC 主目标标志
    uint8_t dataFlag = 1;             ///< GDCL 数据标志
    int32_t fallbackTargetId = 0;     ///< 目标编号无法解析时使用的编号
    int32_t distance = 0;             ///< GDCL 距离字段
    int32_t magnitudeResult = 1;      ///< GDCL 星等计算结果
    bool useGxtcMagnitude = false;    ///< GXTC 是否写入星等，默认兼容旧代码写 0
};

/**
 * @brief 将时间戳转换为旧协议 JMS1970 百分之一秒
 * @param timestamp 通用时间戳
 * @return 自 1970-01-01 00:00:00 起的百分之一秒计数；日期非法时返回 0
 */
[[nodiscard]] inline auto makeJms1970Centiseconds(const Dss::Core::Timestamp& timestamp)
    -> int32_t {
    const auto ymd = std::chrono::year{timestamp.year} /
                     std::chrono::month{static_cast<unsigned int>(timestamp.month)} /
                     std::chrono::day{static_cast<unsigned int>(timestamp.day)};
    if (!ymd.ok() || timestamp.hour < 0 || timestamp.hour > 23 || timestamp.minute < 0 ||
        timestamp.minute > 59 || timestamp.second < 0 || timestamp.second > 59 ||
        timestamp.millisecond < 0 || timestamp.millisecond > 999 || timestamp.microsecond < 0 ||
        timestamp.microsecond > 999) {
        return 0;
    }

    const auto days = std::chrono::sys_days{ymd} -
                      std::chrono::sys_days{std::chrono::year{1970} / std::chrono::January / 1};
    const auto centiseconds = days.count() * 24LL * 3600LL * 100LL +
                              static_cast<int64_t>(timestamp.hour) * 3600LL * 100LL +
                              static_cast<int64_t>(timestamp.minute) * 60LL * 100LL +
                              static_cast<int64_t>(timestamp.second) * 100LL +
                              timestamp.millisecond / 10LL + timestamp.microsecond / 10000LL;
    return static_cast<int32_t>(centiseconds);
}

namespace Detail {

inline constexpr double DegreeToArcsecond = 3600.0;  ///< 度到角秒的换算系数

/**
 * @brief 判断字符是否为 ASCII 数字
 * @param value 待判断字符
 * @return 是数字时返回 true
 */
[[nodiscard]] inline auto isAsciiDigit(char value) -> bool {
    return value >= '0' && value <= '9';
}

/**
 * @brief 将角度换算为旧协议整数角秒
 * @param degrees 角度值（度）
 * @return 直接截断后的角秒值
 */
[[nodiscard]] inline auto degreesToArcseconds(double degrees) -> int32_t {
    return static_cast<int32_t>(degrees * DegreeToArcsecond);
}

/**
 * @brief 判断二维向量是否带有非零值
 * @param value 待判断向量
 * @return 任一分量非零时返回 true
 */
[[nodiscard]] inline auto hasVectorValue(const Dss::Core::Vec2f& value) -> bool {
    return value.x != 0.0F || value.y != 0.0F;
}

/**
 * @brief 选择 GXTC 使用的轴系指向角
 * @param packet 通用测量结果包
 * @return 轴系方位/俯仰角（度）
 */
[[nodiscard]] inline auto selectGxtcPointing(const Dss::Core::ResultPacket& packet)
    -> Dss::Core::Vec2f {
    if (hasVectorValue(packet.targetPosZxdw)) {
        return packet.targetPosZxdw;
    }
    if (hasVectorValue(packet.blob.posAe)) {
        return packet.blob.posAe;
    }
    return Dss::Core::Vec2f{packet.blob.targetAzi, packet.blob.targetEle};
}

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
 * @brief 解析旧协议整数目标编号
 *
 * @details 新跟踪链路的目标标识可能是 `geo-1`、`leo-2` 或 `manual`。旧 GXTC/GDCL
 * 协议只接受整数目标编号，因此这里解析末尾连续数字；没有数字或数值越界时返回 fallback。
 *
 * @param targetId 通用目标标识
 * @param fallback 无法解析时使用的目标编号
 * @return 旧协议整数目标编号
 */
[[nodiscard]] inline auto parseLegacyTargetId(std::string_view targetId, int32_t fallback = 0)
    -> int32_t {
    if (targetId.empty() || !Detail::isAsciiDigit(targetId.back())) {
        return fallback;
    }

    size_t end = targetId.size();
    size_t begin = end;
    while (begin > 0 && Detail::isAsciiDigit(targetId[begin - 1U])) {
        --begin;
    }

    const auto digits = targetId.substr(begin, end - begin);
    int32_t value = fallback;
    const auto* first = digits.data();
    const auto* last = first + digits.size();
    const auto [next, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || next != last) {
        return fallback;
    }
    return value;
}

/**
 * @brief 从通用结果包构造 GXTC 头部元数据
 * @param packet 通用测量结果包
 * @param options 映射选项
 * @return GXTC 头部元数据
 */
[[nodiscard]] inline auto makeGxtcMetadata(const Dss::Core::ResultPacket& packet,
                                           const DataExchangeMappingOptions& options = {})
    -> GxtcMetadata {
    return {
        .jms1970Centiseconds = options.jms1970Centiseconds,
        .measureStatus = options.measureStatus,
        .taskMode = options.taskMode,
        .targetStatus = packet.valid ? options.validTargetStatus : options.invalidTargetStatus,
        .temperatureCelsius = packet.temperature,
        .atmospherePressure = packet.atmosPressure,
        .humidityPercent = static_cast<double>(packet.humidity) * 100.0,
    };
}

/**
 * @brief 从通用结果包构造 GXTC 目标记录
 * @param packet 通用测量结果包
 * @param options 映射选项
 * @return GXTC 单目标记录
 */
[[nodiscard]] inline auto makeGxtcTarget(const Dss::Core::ResultPacket& packet,
                                         const DataExchangeMappingOptions& options = {})
    -> GxtcTarget {
    const auto pointing = Detail::selectGxtcPointing(packet);
    return {
        .mainFlag = options.mainFlag,
        .targetId = parseLegacyTargetId(packet.targetId, options.fallbackTargetId),
        .azimuthArcsec = Detail::degreesToArcseconds(pointing.x),
        .elevationArcsec = Detail::degreesToArcseconds(pointing.y),
        .azimuthSpeedArcsecPerSecond = Detail::degreesToArcseconds(packet.targetSpdAe.x),
        .elevationSpeedArcsecPerSecond = Detail::degreesToArcseconds(packet.targetSpdAe.y),
        .magnitude = options.useGxtcMagnitude ? packet.targetMvGdcl : 0.0,
    };
}

/**
 * @brief 批量构造 GXTC 目标记录
 * @param packets 通用测量结果包列表
 * @param options 映射选项
 * @return GXTC 目标记录列表
 */
[[nodiscard]] inline auto makeGxtcTargets(std::span<const Dss::Core::ResultPacket> packets,
                                          const DataExchangeMappingOptions& options = {})
    -> std::vector<GxtcTarget> {
    std::vector<GxtcTarget> targets;
    targets.reserve(packets.size());
    for (const auto& packet : packets) {
        targets.push_back(makeGxtcTarget(packet, options));
    }
    return targets;
}

/**
 * @brief 从通用结果包构造 GDCL 测量结果
 * @param packet 通用测量结果包
 * @param options 映射选项
 * @return GDCL 测量结果
 */
[[nodiscard]] inline auto makeGdclMeasurement(const Dss::Core::ResultPacket& packet,
                                              const DataExchangeMappingOptions& options = {})
    -> GdclMeasurement {
    return {
        .jms1970Centiseconds = options.jms1970Centiseconds,
        .measureStatus = options.measureStatus,
        .targetId = parseLegacyTargetId(packet.targetId, options.fallbackTargetId),
        .dataFlag = options.dataFlag,
        .dn = static_cast<int32_t>(packet.blob.dn),
        .magnitudeCentivalue = static_cast<int32_t>(packet.targetMvGdcl * 100.0F),
        .distance = options.distance,
        .alphaArcsec = Detail::degreesToArcseconds(packet.targetPosTwdw.x),
        .deltaArcsec = Detail::degreesToArcseconds(packet.targetPosTwdw.y),
        .magnitudeResult = options.magnitudeResult,
    };
}

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
