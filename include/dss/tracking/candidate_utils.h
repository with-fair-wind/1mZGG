#pragma once

#include <cstddef>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Tracking {

/// 初始候选目标去重规则
struct InitialMeasurementDedupRule {
    std::size_t frameCount = 0U;  ///< 参与比对的初始帧数量
};

/**
 * @brief 判断两个候选目标在初始帧中是否存在相同质心的测量
 * @param first 第一个候选目标
 * @param second 第二个候选目标
 * @param rule 去重规则（指定比对帧数）
 * @return 若存在同帧同质心则返回 true
 */
[[nodiscard]] bool sharesInitialCentroidAtSameFrameIndex(const Core::TargetInfo& first,
                                                         const Core::TargetInfo& second,
                                                         const InitialMeasurementDedupRule& rule);

/**
 * @brief 按初始帧质心对候选目标去重，保留先出现的候选
 * @param candidates 待去重的候选目标列表
 * @param rule 去重规则
 * @return 去重后的候选列表
 */
[[nodiscard]] auto deduplicateInitialCandidatesByCentroid(std::vector<Core::TargetInfo> candidates,
                                                          const InitialMeasurementDedupRule& rule)
    -> std::vector<Core::TargetInfo>;

}  // namespace Dss::Tracking
