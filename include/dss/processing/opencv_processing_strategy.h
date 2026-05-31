#pragma once

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing
{

struct OpenCvProcessingOptions
{
    double thresholdSigma = 3.0;
    int minArea = 3;
    int maxArea = 100000;
};

class OpenCvProcessingStrategy final : public IProcessingStrategy
{
public:
    explicit OpenCvProcessingStrategy(OpenCvProcessingOptions options = {});

    [[nodiscard]] auto process(const FramePacket& input) -> ProcessingResult override;
    [[nodiscard]] auto name() const -> std::string_view override;
    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override;

private:
    OpenCvProcessingOptions m_options;
};

} // namespace Dss::Processing
