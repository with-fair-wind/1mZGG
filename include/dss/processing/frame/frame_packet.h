#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Processing {

using RawImageBuffer = std::vector<uint16_t>;                  ///< 16 位 RAW 像素缓冲
using SharedRawImage = std::shared_ptr<const RawImageBuffer>;  ///< 不可变共享 RAW 缓冲

/**
 * @brief 将已构建的 RAW 缓冲转为不可变共享所有权。
 * @param pixels 待共享的像素缓冲。
 *
 * @return 不可变共享 RAW 缓冲。
 */
[[nodiscard]] inline auto makeSharedRawImage(RawImageBuffer pixels) -> SharedRawImage {
    return std::make_shared<const RawImageBuffer>(std::move(pixels));
}

/// 帧数据包，承载单帧图像及其处理中间结果
struct FramePacket {
    uint64_t frameSeq = 0;  ///< 帧序号
    uint32_t width = 0;     ///< 图像宽度（像素）
    uint32_t height = 0;    ///< 图像高度（像素）

    Dss::Core::ExposureDisplayData metadata{};  ///< 曝光与显示元数据
    Dss::Core::ImageStats stats{};              ///< 图像统计信息

    SharedRawImage rawImage;             ///< 原始 16 位灰度图像（跨模块共享）
    std::vector<uint16_t> rotatedImage;  ///< 旋转校正后的 16 位图像
    std::vector<uint8_t> displayImage;   ///< 8 位显示用图像
    std::vector<float> photometryImage;  ///< 测光用浮点图像

    std::vector<Dss::Core::MeasuredBlob> targetBlobs;           ///< 检测到的目标光斑
    std::vector<Dss::Core::MeasuredBlob> validatedTargetBlobs;  ///< 校验通过的目标光斑
    std::vector<Dss::Core::MeasuredBlob> starBlobs;             ///< 检测到的恒星光斑
};

}  // namespace Dss::Processing
