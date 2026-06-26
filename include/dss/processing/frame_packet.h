#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Processing {

/// 帧数据包，承载单帧图像及其处理中间结果
struct FramePacket {
    uint64_t frameSeq = 0;  ///< 帧序号
    uint32_t width = 0;     ///< 图像宽度（像素）
    uint32_t height = 0;    ///< 图像高度（像素）

    Dss::Core::ExposureDisplayData metadata{};  ///< 曝光与显示元数据
    Dss::Core::ImageStats stats{};              ///< 图像统计信息

    std::vector<uint16_t> rawImage;      ///< 原始 16 位灰度图像
    std::vector<uint16_t> rotatedImage;  ///< 旋转校正后的 16 位图像
    std::vector<uint8_t> displayImage;   ///< 8 位显示用图像
    std::vector<float> photometryImage;  ///< 测光用浮点图像

    std::vector<Dss::Core::MeasuredBlob> targetBlobs;           ///< 检测到的目标光斑
    std::vector<Dss::Core::MeasuredBlob> validatedTargetBlobs;  ///< 校验通过的目标光斑
    std::vector<Dss::Core::MeasuredBlob> starBlobs;             ///< 检测到的恒星光斑
};

}  // namespace Dss::Processing
