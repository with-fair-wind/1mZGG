#pragma once

#include "dss/core/constants.h"
#include "dss/core/types.h"

#include <string_view>
#include <vector>

namespace Dss::Processing
{

struct ProcessingResult
{
    Dss::Core::ImageStats stats{};
    std::vector<Dss::Core::MeasuredBlob> targetBlobs;
    std::vector<Dss::Core::MeasuredBlob> starBlobs;
    bool success = false;
};

class IProcessingStrategy
{
public:
    virtual ~IProcessingStrategy() = default;

    [[nodiscard]] virtual auto process(const Dss::Core::FrameMeasurements& input) -> ProcessingResult = 0;
    [[nodiscard]] virtual auto name() const -> std::string_view = 0;
    [[nodiscard]] virtual auto mode() const -> Dss::Core::ProcessingMode = 0;
};

} // namespace Dss::Processing
