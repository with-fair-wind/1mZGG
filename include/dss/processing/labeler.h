#pragma once

#include "dss/core/types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Dss::Processing
{

struct LabelConfig
{
    int minArea = 3;
    int maxArea = 500;
};

class Labeler
{
public:
    explicit Labeler(LabelConfig config = {});

    auto labelAndExtract(std::span<const uint8_t> binaryImage,
                         uint32_t width,
                         uint32_t height) -> std::vector<Dss::Core::MeasuredBlob>;

    void setConfig(LabelConfig config) { m_config = config; }

private:
    LabelConfig m_config;
};

} // namespace Dss::Processing
