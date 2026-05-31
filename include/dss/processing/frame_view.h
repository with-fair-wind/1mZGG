#pragma once

#include "dss/processing/frame_packet.h"

#include <cstdint>
#include <span>

namespace Dss::Processing
{

struct FrameView
{
    uint64_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::span<const uint16_t> rawImage;
    std::span<const uint16_t> rotatedImage;
    std::span<const uint8_t> displayImage;
    std::span<const float> photometryImage;
};

struct MutableFrameView
{
    uint64_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::span<uint16_t> rawImage;
    std::span<uint16_t> rotatedImage;
    std::span<uint8_t> displayImage;
    std::span<float> photometryImage;
};

[[nodiscard]] inline auto makeFrameView(const FramePacket& packet) -> FrameView
{
    return FrameView{
        packet.frameSeq,
        packet.width,
        packet.height,
        packet.rawImage,
        packet.rotatedImage,
        packet.displayImage,
        packet.photometryImage,
    };
}

[[nodiscard]] inline auto makeMutableFrameView(FramePacket& packet) -> MutableFrameView
{
    return MutableFrameView{
        packet.frameSeq,
        packet.width,
        packet.height,
        packet.rawImage,
        packet.rotatedImage,
        packet.displayImage,
        packet.photometryImage,
    };
}

} // namespace Dss::Processing
