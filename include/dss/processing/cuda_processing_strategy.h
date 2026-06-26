#pragma once

#include <expected>
#include <memory>
#include <string>

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing {

/// @brief CUDA 处理后端的检测与显示参数。
struct CudaProcessingOptions {
    double thresholdSigma = 3.0;   ///< 检测阈值的标准差倍数
    int minArea = 3;               ///< 连通域最小面积
    int maxArea = 100000;          ///< 连通域最大面积
    uint16_t displayLow = 0;       ///< 显示拉伸输入下限
    uint16_t displayHigh = 16384;  ///< 显示拉伸输入上限
};

/**
 * @brief 创建 CUDA 图像处理策略。
 * @param options CUDA 检测与显示参数。
 * @return 创建成功时返回策略所有权；CUDA 不可用或初始化失败时返回错误描述。
 */
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
