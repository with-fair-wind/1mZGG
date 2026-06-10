#pragma once

#include <cstdint>
#include <span>

#include "dss/processing/frame_packet.h"

namespace Dss::Processing {

/// 只读帧视图，通过 span 引用 FramePacket 中的图像数据
struct FrameView {
    uint64_t frameSeq = 0;              ///< 帧序号
    uint32_t width = 0;                 ///< 图像宽度（像素）
    uint32_t height = 0;                ///< 图像高度（像素）
    std::span<const uint16_t> rawImage;         ///< 原始 16 位灰度图像视图
    std::span<const uint16_t> rotatedImage;     ///< 旋转校正后的 16 位图像视图
    std::span<const uint8_t> displayImage;      ///< 8 位显示用图像视图
    std::span<const float> photometryImage;     ///< 测光用浮点图像视图
};

/// 可写帧视图，通过 span 引用 FramePacket 中的图像数据
struct MutableFrameView {
    uint64_t frameSeq = 0;              ///< 帧序号
    uint32_t width = 0;                 ///< 图像宽度（像素）
    uint32_t height = 0;                ///< 图像高度（像素）
    std::span<uint16_t> rawImage;         ///< 原始 16 位灰度图像视图
    std::span<uint16_t> rotatedImage;     ///< 旋转校正后的 16 位图像视图
    std::span<uint8_t> displayImage;      ///< 8 位显示用图像视图
    std::span<float> photometryImage;     ///< 测光用浮点图像视图
};

/**
 * @brief 从帧数据包构造只读帧视图
 * @param packet 源帧数据包
 * @return 引用 packet 内部缓冲区的只读视图
 */
[[nodiscard]] inline auto makeFrameView(const FramePacket& packet) -> FrameView {
    return FrameView{
        packet.frameSeq,     packet.width,        packet.height,          packet.rawImage,
        packet.rotatedImage, packet.displayImage, packet.photometryImage,
    };
}

/**
 * @brief 从帧数据包构造可写帧视图
 * @param packet 源帧数据包
 * @return 引用 packet 内部缓冲区的可写视图
 */
[[nodiscard]] inline auto makeMutableFrameView(FramePacket& packet) -> MutableFrameView {
    return MutableFrameView{
        packet.frameSeq,     packet.width,        packet.height,          packet.rawImage,
        packet.rotatedImage, packet.displayImage, packet.photometryImage,
    };
}

}  // namespace Dss::Processing
