#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Dss::Acquisition {

/// 相机控制指令，固定 3 字节
using CameraCommand = std::array<uint8_t, 3>;

/**
 * @brief 构建采集启停指令
 * @param start true 表示开始采集，false 表示停止采集
 * @return 3 字节采集控制指令
 */
[[nodiscard]] inline auto buildGrabCommand(bool start) -> CameraCommand {
    return start ? CameraCommand{0x51, 0x47, 0x03} : CameraCommand{0x51, 0x07, 0x03};
}

/**
 * @brief 将寄存器写入参数编码为单条相机指令
 * @param registerBank 寄存器组编号
 * @param address 寄存器地址
 * @param data 写入数据
 * @return 3 字节寄存器写入指令
 */
[[nodiscard]] inline auto encodeRegisterCommand(uint8_t registerBank, uint8_t address, uint8_t data)
    -> CameraCommand {
    const auto value = (static_cast<uint32_t>(data) << 8U) | static_cast<uint32_t>(address);
    return CameraCommand{
        static_cast<uint8_t>((value << 4U) | (static_cast<uint32_t>(registerBank) << 2U) | 1U),
        static_cast<uint8_t>(((value >> 4U) << 2U) | 3U),
        static_cast<uint8_t>(((value >> 10U) << 2U) | 3U),
    };
}

/**
 * @brief 构建曝光时间寄存器指令序列
 * @param exposureMilliseconds 曝光时间（毫秒），最小值截断为 1.3ms
 * @return 3 条寄存器写入指令，分别写入曝光计数的低、中、高字节
 */
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

/**
 * @brief 构建增益寄存器指令序列
 * @param gain 增益值（当前实现忽略该参数，使用固定 legacy 增益）
 * @return 3 条寄存器写入指令
 */
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

/**
 * @brief 构建 ROI 窗口寄存器指令序列
 * @param line 行数（全幅或子窗口高度）
 * @param start 起始行偏移
 * @param fullWidth 全幅行数，用于判定全幅模式
 * @param subWidth 子窗口行数，用于判定子窗口模式
 * @return 4 条寄存器写入指令；参数不匹配时返回空向量
 */
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

/**
 * @brief 从相机回传数据中解码温度值
 * @param data 至少 3 字节的温度回传数据
 * @return 摄氏度温度值；数据不足时返回 std::nullopt
 */
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
