#pragma once

#include <cstdint>
#include <vector>

#include "dss/processing/i_processing_strategy.h"
#include "dss/processing/labeler.h"

namespace Dss::Processing {

/// @brief 相邻帧差分检测的阈值与面积约束。
struct DiffProcessingOptions {
    std::uint16_t threshold = 20;  ///< 像素绝对差阈值
    int minArea = 3;               ///< 连通域最小面积
    int maxArea = 100000;          ///< 连通域最大面积
};

/// @brief 通过相邻帧绝对差提取运动目标的处理策略。
class DiffProcessingStrategy final : public IProcessingStrategy {
public:
    /**
     * @brief 创建差分处理策略。
     * @param options 差分阈值与连通域面积约束。
     */
    explicit DiffProcessingStrategy(DiffProcessingOptions options = {});

    [[nodiscard]] auto process(const FramePacket& input) -> ProcessingResult override;
    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override;

private:
    DiffProcessingOptions m_options;             ///< 当前差分检测参数
    Labeler m_labeler;                           ///< 二值差分图连通域标注器
    std::vector<std::uint16_t> m_previousImage;  ///< 上一帧 16 位像素副本
    std::uint32_t m_previousWidth = 0;           ///< 上一帧宽度
    std::uint32_t m_previousHeight = 0;          ///< 上一帧高度
};

}  // namespace Dss::Processing
