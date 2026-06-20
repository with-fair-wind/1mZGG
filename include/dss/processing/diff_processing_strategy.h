#pragma once

#include <cstdint>
#include <vector>

#include "dss/processing/i_processing_strategy.h"
#include "dss/processing/labeler.h"

namespace Dss::Processing {

struct DiffProcessingOptions {
    std::uint16_t threshold = 20;
    int minArea = 3;
    int maxArea = 100000;
};

class DiffProcessingStrategy final : public IProcessingStrategy {
public:
    explicit DiffProcessingStrategy(DiffProcessingOptions options = {});

    [[nodiscard]] auto process(const FramePacket& input) -> ProcessingResult override;
    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override;

private:
    DiffProcessingOptions m_options;
    Labeler m_labeler;
    std::vector<std::uint16_t> m_previousImage;
    std::uint32_t m_previousWidth = 0;
    std::uint32_t m_previousHeight = 0;
};

}  // namespace Dss::Processing
