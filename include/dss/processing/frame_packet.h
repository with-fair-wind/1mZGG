#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Processing {

struct FramePacket {
    uint64_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    Dss::Core::ExposureDisplayData metadata{};
    Dss::Core::ImageStats stats{};

    std::vector<uint16_t> rawImage;
    std::vector<uint16_t> rotatedImage;
    std::vector<uint8_t> displayImage;
    std::vector<float> photometryImage;

    std::vector<Dss::Core::MeasuredBlob> targetBlobs;
    std::vector<Dss::Core::MeasuredBlob> starBlobs;
};

}  // namespace Dss::Processing
