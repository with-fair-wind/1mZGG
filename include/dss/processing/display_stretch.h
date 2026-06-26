#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Processing {

/// 显示拉伸模式。
enum class DisplayStretchMode {
    Auto,    ///< 按图像统计量自动计算低/高阈值
    Manual,  ///< 使用用户指定的低/高阈值
};

/// 16 位图像到 8 位显示图的拉伸参数。
struct DisplayStretchSettings {
    DisplayStretchMode mode = DisplayStretchMode::Auto;  ///< 当前拉伸模式
    std::uint16_t low = 1000;                             ///< 手动低阈值
    std::uint16_t high = 5000;                            ///< 手动高阈值
    std::uint16_t signalMin = 0;                          ///< 允许的最小信号值
    std::uint16_t signalMax = 16384;                      ///< 允许的最大信号值
    double sigma = 3.0;                                   ///< 自动拉伸标准差倍率
};

/// 实际用于显示映射的低/高阈值窗口。
struct DisplayStretchWindow {
    std::uint16_t low = 0;   ///< 映射到 0 的低阈值
    std::uint16_t high = 1;  ///< 映射到 255 的高阈值
};

/// 显示拉伸输出结果。
struct DisplayStretchResult {
    Dss::Core::ImageStats stats{};             ///< 输入 RAW 图像统计量
    DisplayStretchWindow window{};             ///< 实际使用的显示窗口
    std::vector<std::uint8_t> displayImage;    ///< 8 位显示图像
};

/**
 * @brief 计算 16 位灰度图的统计量。
 * @param raw 16 位 RAW 图像
 * @return 最小值、最大值、均值与标准差
 */
[[nodiscard]] auto computeImageStats(std::span<const std::uint16_t> raw)
    -> Dss::Core::ImageStats;

/**
 * @brief 根据统计量和拉伸设置计算显示窗口。
 * @param stats 图像统计量
 * @param settings 拉伸设置
 * @return 实际低/高阈值
 */
[[nodiscard]] auto resolveDisplayStretchWindow(const Dss::Core::ImageStats& stats,
                                               const DisplayStretchSettings& settings)
    -> DisplayStretchWindow;

/**
 * @brief 使用指定低/高阈值将 16 位图像拉伸为 8 位显示图。
 * @param raw 16 位 RAW 图像
 * @param window 显示窗口
 * @return 8 位显示图像
 */
[[nodiscard]] auto stretchDisplayImage(std::span<const std::uint16_t> raw,
                                       DisplayStretchWindow window)
    -> std::vector<std::uint8_t>;

/**
 * @brief 计算统计量、解析窗口并生成 8 位显示图。
 * @param raw 16 位 RAW 图像
 * @param settings 拉伸设置
 * @return 显示拉伸结果
 */
[[nodiscard]] auto buildDisplayImage(std::span<const std::uint16_t> raw,
                                     const DisplayStretchSettings& settings)
    -> DisplayStretchResult;

}  // namespace Dss::Processing
