#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Tracking {

/**
 * @brief 候选目标测量比较所使用的坐标空间。
 */
enum class CandidateMeasurementSpace {
    Centroid,  ///< 使用像面质心坐标比较测量点。
    RaDec,     ///< 使用赤经/赤纬坐标比较测量点。
};

/**
 * @brief 初始候选目标去重规则。
 */
struct InitialMeasurementDedupRule {
    std::size_t frameCount = 0U;  ///< 参与比对的初始帧数量。
};

/**
 * @brief 最近帧测量重叠判断规则。
 */
struct RecentMeasurementOverlapRule {
    std::size_t frameCount = 0U;  ///< 参与比对的最近帧数量。
    CandidateMeasurementSpace space =
        CandidateMeasurementSpace::Centroid;  ///< 测量点比较所使用的坐标空间。
};

/**
 * @brief 判断两个光斑在指定测量空间中是否表示同一测量点。
 * @param first 第一个光斑。
 * @param second 第二个光斑。
 * @param space 测量点比较所使用的坐标空间。
 * @return 若两个光斑在指定空间中相同则返回 true。
 */
[[nodiscard]] bool hasSameMeasurement(const Core::MeasuredBlob& first,
                                      const Core::MeasuredBlob& second,
                                      CandidateMeasurementSpace space);

/**
 * @brief 判断两个帧级目标信息在指定测量空间中是否表示同一测量点。
 * @param first 第一个帧级目标信息。
 * @param second 第二个帧级目标信息。
 * @param space 测量点比较所使用的坐标空间。
 * @return 若两个帧级目标信息在指定空间中相同则返回 true。
 */
[[nodiscard]] bool hasSameMeasurement(const Core::TargetFrameInfo& first,
                                      const Core::TargetFrameInfo& second,
                                      CandidateMeasurementSpace space);

/**
 * @brief 判断两个候选目标在初始帧中是否存在同索引的相同测量。
 * @param first 第一个候选目标。
 * @param second 第二个候选目标。
 * @param rule 初始候选目标去重规则。
 * @param space 测量点比较所使用的坐标空间。
 * @return 若存在同一初始帧索引下的相同测量则返回 true。
 */
[[nodiscard]] bool sharesInitialMeasurementAtSameFrameIndex(const Core::TargetInfo& first,
                                                            const Core::TargetInfo& second,
                                                            const InitialMeasurementDedupRule& rule,
                                                            CandidateMeasurementSpace space);

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
 * @brief 判断候选目标与已有目标最近帧窗口中是否存在同索引的相同测量。
 * @param candidate 待检查的候选目标。
 * @param existingTarget 已有目标。
 * @param rule 最近帧测量重叠判断规则。
 * @return 若两个目标的最近帧窗口存在同索引同帧号的相同测量则返回 true。
 */
[[nodiscard]] bool sharesRecentMeasurementAtSameFrameIndex(
    const Core::TargetInfo& candidate, const Core::TargetInfo& existingTarget,
    const RecentMeasurementOverlapRule& rule);

/**
 * @brief 判断候选目标是否与任一存活目标的最近帧测量重叠。
 * @param candidate 待检查的候选目标。
 * @param targets 已有目标集合。
 * @param rule 最近帧测量重叠判断规则。
 * @return 若候选目标与任一 living 目标重叠则返回 true。
 */
[[nodiscard]] bool overlapsAnyLivingTarget(const Core::TargetInfo& candidate,
                                           std::span<const Core::TargetInfo> targets,
                                           const RecentMeasurementOverlapRule& rule);

/**
 * @brief 按指定测量空间对初始候选目标去重，保留先出现的候选。
 * @param candidates 待去重的候选目标列表。
 * @param rule 初始候选目标去重规则。
 * @param space 测量点比较所使用的坐标空间。
 * @return 去重后的候选列表。
 */
[[nodiscard]] auto deduplicateInitialCandidates(std::vector<Core::TargetInfo> candidates,
                                                const InitialMeasurementDedupRule& rule,
                                                CandidateMeasurementSpace space)
    -> std::vector<Core::TargetInfo>;

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
