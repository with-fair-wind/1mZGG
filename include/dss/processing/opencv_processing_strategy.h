#pragma once

#include <cstdint>

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing {

/// OpenCV 处理策略的可配置参数
struct OpenCvProcessingOptions {
    double thresholdSigma = 3.0;        ///< 二值化阈值偏移（均值 + sigma × 标准差）
    int minArea = 3;                    ///< 连通域最小面积（像素）
    int maxArea = 100000;               ///< 连通域最大面积（像素）
    std::uint16_t displayLow = 0;       ///< 显示拉伸输入下限
    std::uint16_t displayHigh = 16384;  ///< 显示拉伸输入上限
};

/// 基于 OpenCV 的图像处理策略，执行统计、归一化与连通域检测
class OpenCvProcessingStrategy final : public IProcessingStrategy {
public:
    /**
     * @brief 构造 OpenCV 处理策略
     * @param options 处理参数，默认使用内置默认值
     */
    explicit OpenCvProcessingStrategy(OpenCvProcessingOptions options = {});

    /**
     * @brief 处理单帧图像
     * @param input 输入帧数据包
     * @return 处理结果
     */
    [[nodiscard]] auto process(const FramePacket& input) -> ProcessingResult override;

    /** @brief 获取策略名称。 @return 固定返回 "opencv"。 */
    [[nodiscard]] auto name() const -> std::string_view override;

    /**
     * @brief 获取策略处理模式。
     * @return 固定返回 ProcessingMode::Direct。
     */
    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override;

private:
    OpenCvProcessingOptions m_options;  ///< 当前处理参数
};

}  // namespace Dss::Processing
