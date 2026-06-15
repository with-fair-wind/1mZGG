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
#include <utility>

#include "dss/comm/frame_codec.h"
#include "dss/core/events.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 串口通信协议类型
enum class SerialProtocol {
    Display,        ///< 显示通道协议
    Exposure,       ///< 曝光通道协议
    MasterControl,  ///< 主控通道协议
    Servo,          ///< 伺服通道协议
};

/// 各协议帧的布局描述
struct SerialFrameLayout {
    SerialProtocol protocol;   ///< 协议类型
    std::string_view name;     ///< 协议名称（用于错误信息）
    std::size_t recvSize = 0;  ///< 接收帧字节长度
    std::size_t sendSize = 0;  ///< 发送帧字节长度
};

/// 四种协议帧布局的查找表
inline constexpr std::array<SerialFrameLayout, 4> SerialFrameLayouts{
    SerialFrameLayout{SerialProtocol::Display, "display", 20, 9},
    SerialFrameLayout{SerialProtocol::Exposure, "exposure", 23, 8},
    SerialFrameLayout{SerialProtocol::MasterControl, "master_control", 30, 28},
    SerialFrameLayout{SerialProtocol::Servo, "servo", 20, 14},
};

/**
 * @brief 根据协议类型查找帧布局
 * @param protocol 协议类型
 * @return 匹配的帧布局；未找到时返回空布局
 */
[[nodiscard]] constexpr auto layoutFor(SerialProtocol protocol) -> SerialFrameLayout {
    for (const auto& layout : SerialFrameLayouts) {
        if (layout.protocol == protocol) {
            return layout;
        }
    }
    return {};
}

/**
 * @brief 校验接收帧的头尾与长度
 * @param protocol 协议类型
 * @param frame 待校验的帧数据
 * @return 帧合法时返回 true
 */
[[nodiscard]] inline bool validateReceiveFrame(SerialProtocol protocol,
                                               std::span<const uint8_t> frame) {
    return FrameCodec::validate(frame, layoutFor(protocol).recvSize);
}

/**
 * @brief 为发送帧写入帧头尾
 * @param protocol 协议类型
 * @param frame 待封装的帧缓冲区
 * @return 缓冲区长度匹配时返回 true
 */
inline bool prepareSendFrame(SerialProtocol protocol, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(protocol).sendSize) {
        return false;
    }
    FrameCodec::wrap(frame);
    return true;
}

/// 曝光触发模式
enum class ExposureTriggerMode {
    External,  ///< 外部触发
    FreeRun,   ///< 自由运行
};

/// 曝光通道发送命令参数
struct ExposureCommand {
    ExposureTriggerMode triggerMode = ExposureTriggerMode::External;  ///< 触发模式
    uint8_t frameFrequencyCode = 0x01;                                ///< 帧频编码
    uint32_t exposureDelayTicks = 0;                                  ///< 曝光延迟（时钟节拍）
};

/// 主控通道接收到的指令
struct MasterControlCommand {
    float exposure = 0.0F;         ///< 曝光时间
    uint8_t mode1 = 0;             ///< 主控模式字 1
    uint8_t mode2 = 0;             ///< 主控模式字 2
    int trackMode = -1;            ///< 跟踪模式（由 mode1/mode2 映射）
    bool track = false;            ///< 是否跟踪
    bool save = false;             ///< 是否保存
    bool grab = false;             ///< 是否抓取
    uint32_t targetId = 0;         ///< 目标编号
    uint32_t taskId = 0;           ///< 任务编号
    Dss::Core::TimeOfDay start{};  ///< 任务开始时刻
    Dss::Core::TimeOfDay end{};    ///< 任务结束时刻
};

/// 伺服修正量
struct ServoCorrection {
    bool distanceValid = false;            ///< 距离修正量是否有效
    bool speedValid = false;               ///< 速度修正量是否有效
    Dss::Core::Vec2f distanceArcsec{};     ///< 距离修正（角秒）
    Dss::Core::Vec2f speedArcsecPerSec{};  ///< 速度修正（角秒/秒）
    uint8_t mode = 0x19;                   ///< 伺服工作模式
};

/// 主控通道发送的状态帧内容
struct MasterControlStatus {
    ServoCorrection correction{};      ///< 伺服修正量
    Dss::Core::Timestamp timestamp{};  ///< 时间戳
    Dss::Core::Vec2f pointingAe{};     ///< 指向角（方位/俯仰，度）
};

/// 串口协议字段级解码错误
struct SerialDecodeError {
    std::string message;         ///< 可展示的错误描述
    std::string field;           ///< 发生错误的协议字段名称
    std::size_t byteOffset = 0;  ///< 字段起始字节偏移
    uint64_t rawValue = 0;       ///< 字段原始值或解码后的异常值
};

/// 编解码内部辅助函数与常量
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

/// @brief 解码显示通道接收帧并保留字段级错误。
/// @param frame 原始帧数据。
/// @return 解码后的曝光显示数据，失败时返回字段级错误。
[[nodiscard]] inline auto decodeDisplayFrameDetailed(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, SerialDecodeError> {
    return detail::decodePointingFrame(frame, SerialProtocol::Display);
}

/// @brief 解码显示通道接收帧。
/// @param frame 原始帧数据。
/// @return 解码后的曝光显示数据，失败时返回错误描述。
[[nodiscard]] inline auto decodeDisplayFrame(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, std::string> {
    auto decoded = decodeDisplayFrameDetailed(frame);
    if (!decoded) {
        return std::unexpected(decoded.error().message);
    }
    return *decoded;
}

/// @brief 解码曝光通道接收帧并保留字段级错误。
/// @param frame 原始帧数据。
/// @return 解码后的曝光显示数据，失败时返回字段级错误。
[[nodiscard]] inline auto decodeExposureFrameDetailed(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, SerialDecodeError> {
    return detail::decodePointingFrame(frame, SerialProtocol::Exposure);
}

/// @brief 解码曝光通道接收帧。
/// @param frame 原始帧数据。
/// @return 解码后的曝光显示数据，失败时返回错误描述。
[[nodiscard]] inline auto decodeExposureFrame(std::span<const uint8_t> frame)
    -> std::expected<Dss::Core::ExposureDisplayData, std::string> {
    auto decoded = decodeExposureFrameDetailed(frame);
    if (!decoded) {
        return std::unexpected(decoded.error().message);
    }
    return *decoded;
}

/**
 * @brief 将曝光命令编码为发送帧
 * @param command 曝光命令参数
 * @param frame 目标帧缓冲区
 * @return 编码成功时返回 true
 */
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

/**
 * @brief 将 legacy 模式字映射为跟踪模式编号
 * @param mode1 主控模式字 1
 * @param mode2 主控模式字 2
 * @return 跟踪模式编号；-1 表示未知模式
 */
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

/// @brief 解码主控通道接收帧并保留字段级错误。
/// @param frame 原始帧数据。
/// @return 解码后的主控指令，失败时返回字段级错误。
[[nodiscard]] inline auto decodeMasterControlFrameDetailed(std::span<const uint8_t> frame)
    -> std::expected<MasterControlCommand, SerialDecodeError> {
    if (!validateReceiveFrame(SerialProtocol::MasterControl, frame)) {
        return std::unexpected(detail::invalidFrameDecodeError(SerialProtocol::MasterControl));
    }

    auto startHour = detail::decodeRangeField("start.hour", frame[17], 17U, 0, 23);
    if (!startHour) {
        return std::unexpected(std::move(startHour.error()));
    }
    auto startMinute = detail::decodeRangeField("start.minute", frame[18], 18U, 0, 59);
    if (!startMinute) {
        return std::unexpected(std::move(startMinute.error()));
    }
    auto startSecond = detail::decodeRangeField("start.second", frame[19], 19U, 0, 59);
    if (!startSecond) {
        return std::unexpected(std::move(startSecond.error()));
    }
    auto endHour = detail::decodeRangeField("end.hour", frame[20], 20U, 0, 23);
    if (!endHour) {
        return std::unexpected(std::move(endHour.error()));
    }
    auto endMinute = detail::decodeRangeField("end.minute", frame[21], 21U, 0, 59);
    if (!endMinute) {
        return std::unexpected(std::move(endMinute.error()));
    }
    auto endSecond = detail::decodeRangeField("end.second", frame[22], 22U, 0, 59);
    if (!endSecond) {
        return std::unexpected(std::move(endSecond.error()));
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
    command.start = Dss::Core::TimeOfDay{*startHour, *startMinute, *startSecond};
    command.end = Dss::Core::TimeOfDay{*endHour, *endMinute, *endSecond};
    return command;
}

/// @brief 解码主控通道接收帧。
/// @param frame 原始帧数据。
/// @return 解码后的主控指令，失败时返回错误描述。
[[nodiscard]] inline auto decodeMasterControlFrame(std::span<const uint8_t> frame)
    -> std::expected<MasterControlCommand, std::string> {
    auto decoded = decodeMasterControlFrameDetailed(frame);
    if (!decoded) {
        return std::unexpected(decoded.error().message);
    }
    return *decoded;
}

/// @brief 将主控指令转换为系统事件。
/// @param command 主控指令。
/// @return 对应的主控事件对象。
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

/**
 * @brief 将伺服修正量编码为发送帧
 * @param correction 伺服修正参数
 * @param frame 目标帧缓冲区
 * @return 编码成功时返回 true
 */
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

/**
 * @brief 将主控状态编码为发送帧
 * @param status 主控状态（含修正量、时间戳、指向角）
 * @param frame 目标帧缓冲区
 * @return 编码成功时返回 true
 */
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
