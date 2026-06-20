#pragma once

#include <expected>
#include <memory>
#include <string>

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing {

struct CudaProcessingOptions {
    double thresholdSigma = 3.0;
    int minArea = 3;
    int maxArea = 100000;
    uint16_t displayLow = 0;
    uint16_t displayHigh = 16384;
};

#ifdef DSS_HAS_CUDA
auto createCudaProcessingStrategy(CudaProcessingOptions options = {})
    -> std::expected<std::unique_ptr<IProcessingStrategy>, std::string>;
#else
inline auto createCudaProcessingStrategy(CudaProcessingOptions = {})
    -> std::expected<std::unique_ptr<IProcessingStrategy>, std::string> {
    return std::unexpected("CUDA processing is not available in this build");
}
#endif

}  // namespace Dss::Processing