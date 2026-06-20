#pragma once

namespace detail {

inline constexpr double AngleCodeDenominator =
    static_cast<double>(1ULL << 29U);  ///< 角度编码分母（2^29）

/**
 * @brief 生成帧校验失败时的错误描述
 * @param protocol 协议类型
 * @return 包含协议名称的错误字符串
 */
[[nodiscard]] inline auto invalidFrameError(SerialProtocol protocol) -> std::string {
    return std::string("invalid ") + std::string(layoutFor(protocol).name) + " frame";
}

/// @brief 构造通用字段级解码错误。
/// @param field 字段名称。
/// @param message 错误描述。
/// @param byteOffset 字节偏移。
/// @param rawValue 原始值或异常值。
/// @return 字段级解码错误。
[[nodiscard]] inline auto makeDecodeError(std::string field, std::string message,
                                          std::size_t byteOffset, uint64_t rawValue)
    -> SerialDecodeError {
    return SerialDecodeError{
        .message = std::move(message),
        .field = std::move(field),
        .byteOffset = byteOffset,
        .rawValue = rawValue,
    };
}

/// @brief 构造固定帧校验失败对应的字段级解码错误。
/// @param protocol 协议类型。
/// @return 字段级解码错误。
[[nodiscard]] inline auto invalidFrameDecodeError(SerialProtocol protocol) -> SerialDecodeError {
    return makeDecodeError("frame", invalidFrameError(protocol), 0U, 0U);
}

/// @brief 从缓冲区读取小端 16 位无符号整数。
/// @param data 源数据。
/// @param offset 起始偏移（字节）。
/// @return 解码后的值。
[[nodiscard]] constexpr auto readU16Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint16_t {
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1U]) << 8U);
}

/**
 * @brief 从缓冲区读取小端 24 位无符号整数
 * @param data 源数据
 * @param offset 起始偏移（字节）
 * @return 解码后的值
 */
[[nodiscard]] constexpr auto readU24Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint32_t {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2U]) << 16U);
}

/**
 * @brief 从缓冲区读取小端 32 位无符号整数
 * @param data 源数据
 * @param offset 起始偏移（字节）
 * @return 解码后的值
 */
[[nodiscard]] constexpr auto readU32Le(std::span<const uint8_t> data, std::size_t offset)
    -> uint32_t {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<uint32_t>(data[offset + 3U]) << 24U);
}

/**
 * @brief 向缓冲区写入小端 16 位无符号整数
 * @param data 目标缓冲区
 * @param offset 起始偏移（字节）
 * @param value 待写入的值
 */
constexpr void writeU16Le(std::span<uint8_t> data, std::size_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

/**
 * @brief 向缓冲区写入小端 24 位无符号整数
 * @param data 目标缓冲区
 * @param offset 起始偏移（字节）
 * @param value 待写入的值
 */
constexpr void writeU24Le(std::span<uint8_t> data, std::size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
}

/**
 * @brief 向缓冲区写入小端 32 位无符号整数
 * @param data 目标缓冲区
 * @param offset 起始偏移（字节）
 * @param value 待写入的值
 */
constexpr void writeU32Le(std::span<uint8_t> data, std::size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

/**
 * @brief 解码 BCD 字节为十进制整数
 * @param value BCD 编码字节
 * @return 0–99 范围内的十进制值
 */
[[nodiscard]] constexpr auto decodeBcd(uint8_t value) -> int {
    return static_cast<int>(((value >> 4U) & 0x0FU) * 10U + (value & 0x0FU));
}

/// @brief 判断字节是否符合 BCD 编码。
/// @param value 待检查字节。
/// @return 高低半字节均为 0-9 时返回 true。
[[nodiscard]] constexpr auto isBcdByte(uint8_t value) -> bool {
    return ((value >> 4U) & 0x0FU) <= 9U && (value & 0x0FU) <= 9U;
}

/// @brief 将单字节格式化为 0xNN 文本。
/// @param value 待格式化字节。
/// @return 十六进制字符串。
[[nodiscard]] inline auto hexByte(uint8_t value) -> std::string {
    constexpr std::array digits{'0', '1', '2', '3', '4', '5', '6', '7',
                                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string text = "0x00";
    text[2] = digits[(value >> 4U) & 0x0FU];
    text[3] = digits[value & 0x0FU];
    return text;
}

/// @brief 解码并校验 BCD 时间字段。
/// @param field 字段名称。
/// @param value BCD 原始值。
/// @param byteOffset 字节偏移。
/// @param minimum 允许的最小十进制值。
/// @param maximum 允许的最大十进制值。
/// @return 解码后的十进制值，失败时返回字段级错误。
[[nodiscard]] inline auto decodeBcdField(std::string field, uint8_t value, std::size_t byteOffset,
                                         int minimum, int maximum)
    -> std::expected<int, SerialDecodeError> {
    if (!isBcdByte(value)) {
        const auto message = field + " has invalid BCD value " + hexByte(value);
        return std::unexpected(makeDecodeError(std::move(field), message, byteOffset, value));
    }

    const auto decoded = decodeBcd(value);
    if (decoded < minimum || decoded > maximum) {
        const auto message = field + " is out of range: " + std::to_string(decoded);
        return std::unexpected(
            makeDecodeError(std::move(field), message, byteOffset, static_cast<uint64_t>(decoded)));
    }
    return decoded;
}

/// @brief 校验普通单字节时间字段范围。
/// @param field 字段名称。
/// @param value 原始值。
/// @param byteOffset 字节偏移。
/// @param minimum 允许的最小值。
/// @param maximum 允许的最大值。
/// @return 校验通过后的值，失败时返回字段级错误。
[[nodiscard]] inline auto decodeRangeField(std::string field, uint8_t value, std::size_t byteOffset,
                                           int minimum, int maximum)
    -> std::expected<int, SerialDecodeError> {
    const auto decoded = static_cast<int>(value);
    if (decoded < minimum || decoded > maximum) {
        const auto message = field + " is out of range: " + std::to_string(decoded);
        return std::unexpected(
            makeDecodeError(std::move(field), message, byteOffset, static_cast<uint64_t>(decoded)));
    }
    return decoded;
}

/// @brief 将十进制整数编码为 BCD 字节。
/// @param value 待编码的十进制值（自动钳制到 0-99）。
/// @return BCD 编码字节。
[[nodiscard]] constexpr auto encodeBcd(int value) -> uint8_t {
    const auto clamped = std::clamp(value, 0, 99);
    return static_cast<uint8_t>(((clamped / 10) << 4U) | (clamped % 10));
}

/**
 * @brief 将角度编码值解码为度数
 * @param code 29 位角度编码
 * @return 0–360 范围内的角度（度）
 */
[[nodiscard]] inline auto decodeAngle(uint32_t code) -> float {
    return static_cast<float>(static_cast<double>(code) / AngleCodeDenominator * 360.0);
}

/**
 * @brief 将度数编码为 29 位角度码
 * @param degrees 角度（度，自动钳制到 0–360）
 * @return 角度编码值
 */
[[nodiscard]] inline auto encodeAngle(float degrees) -> uint32_t {
    const auto normalized = std::clamp(static_cast<double>(degrees), 0.0, 360.0);
    return static_cast<uint32_t>(std::llround(normalized / 360.0 * AngleCodeDenominator));
}

/**
 * @brief 解码指向帧（显示/曝光协议共用格式）
 * @param frame 原始帧数据
 * @param protocol 协议类型（用于校验）
 * @return 解码后的曝光显示数据，失败时返回错误描述
 */
[[nodiscard]] inline auto decodePointingFrame(std::span<const uint8_t> frame,
                                              SerialProtocol protocol)
    -> std::expected<Dss::Core::ExposureDisplayData, SerialDecodeError> {
    if (!validateReceiveFrame(protocol, frame)) {
        return std::unexpected(invalidFrameDecodeError(protocol));
    }

    auto year = decodeBcdField("timestamp.year", frame[1], 1U, 0, 99);
    if (!year) {
        return std::unexpected(std::move(year.error()));
    }
    auto month = decodeBcdField("timestamp.month", frame[2], 2U, 1, 12);
    if (!month) {
        return std::unexpected(std::move(month.error()));
    }
    auto day = decodeBcdField("timestamp.day", frame[3], 3U, 1, 31);
    if (!day) {
        return std::unexpected(std::move(day.error()));
    }
    auto hour = decodeBcdField("timestamp.hour", frame[4], 4U, 0, 23);
    if (!hour) {
        return std::unexpected(std::move(hour.error()));
    }
    auto minute = decodeBcdField("timestamp.minute", frame[5], 5U, 0, 59);
    if (!minute) {
        return std::unexpected(std::move(minute.error()));
    }
    auto second = decodeBcdField("timestamp.second", frame[6], 6U, 0, 59);
    if (!second) {
        return std::unexpected(std::move(second.error()));
    }

    Dss::Core::ExposureDisplayData decoded{};
    decoded.timestamp.year = 2000 + *year;
    decoded.timestamp.month = *month;
    decoded.timestamp.day = *day;
    decoded.timestamp.hour = *hour;
    decoded.timestamp.minute = *minute;
    decoded.timestamp.second = *second;
    decoded.timestamp.millisecond = static_cast<int>(readU16Le(frame, 7U) / 10U);
    decoded.pointingAe.x = decodeAngle(readU32Le(frame, 9U));
    decoded.pointingAe.y = decodeAngle(readU32Le(frame, 13U));
    return decoded;
}

/**
 * @brief 编码 16 位有符号幅值（最高位为符号位）
 * @param frame 目标帧缓冲区
 * @param offset 起始偏移（字节）
 * @param value 待编码的浮点值
 * @param scale 幅值缩放系数
 */
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

/**
 * @brief 编码 24 位有符号幅值（最高位为符号位）
 * @param frame 目标帧缓冲区
 * @param offset 起始偏移（字节）
 * @param value 待编码的浮点值
 */
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
