#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Processing {

/// 连通域标注器的可配置参数
struct LabelConfig {
    int minArea = 3;    ///< 保留光斑的最小面积（像素）
    int maxArea = 500;  ///< 保留光斑的最大面积（像素）
};

/// 二值图像连通域标注器，提取满足面积约束的光斑
class Labeler {
public:
    /**
     * @brief 构造标注器
     * @param config 标注参数，默认使用内置默认值
     */
    explicit Labeler(LabelConfig config = {});

    /**
     * @brief 对二值图像进行连通域标注并提取光斑
     * @param binaryImage 二值图像数据（非零像素视为前景）
     * @param width 图像宽度（像素）
     * @param height 图像高度（像素）
     * @return 满足面积约束的光斑列表；尺寸不匹配时返回空列表
     */
    auto labelAndExtract(std::span<const uint8_t> binaryImage, uint32_t width, uint32_t height)
        -> std::vector<Dss::Core::MeasuredBlob>;

    /// 更新标注参数
    void setConfig(LabelConfig config) {
        m_config = config;
    }

private:
    LabelConfig m_config;  ///< 当前标注参数
};

}  // namespace Dss::Processing
