#pragma once

#include "dss/core/types.h"

namespace Dss::Tracking {

/// 最近帧窗口检查模式
enum class RecentFrameWindowMode {
    RequireFullWindow,  ///< 要求帧数达到完整窗口才判定
    UseAvailableFrames, ///< 使用当前已有的帧数进行判定
};

/// 目标丢失时的存活判定策略
enum class TrackMissPolicy {
    UseValidityWindow,                    ///< 基于有效性窗口与阈值判定
    RequireLatestValid,                   ///< 要求最新帧必须有效
    DropAfterConsecutiveInvalidFrames,    ///< 连续无效帧达到窗口上限则丢弃
};

/// 目标存活判定规则
struct TrackLivingRule {
    int frameWindow = 0;                              ///< 检查的最近帧窗口大小
    float threshold = 0.0F;                           ///< 有效性阈值（用于 UseValidityWindow）
    TrackMissPolicy missPolicy = TrackMissPolicy::UseValidityWindow;  ///< 丢失判定策略
};

/**
 * @brief 统计最近窗口内无效帧的数量
 * @param target 目标轨迹
 * @param frameWindow 检查窗口大小
 * @return 无效帧计数
 */
[[nodiscard]] auto countRecentInvalidFrames(const Core::TargetInfo& target, int frameWindow) -> int;

/**
 * @brief 判断最近窗口内的帧是否全部无效
 * @param target 目标轨迹
 * @param frameWindow 检查窗口大小
 * @param mode 窗口检查模式
 * @return 全部无效时返回 true
 */
[[nodiscard]] bool latestFramesAreAllInvalid(const Core::TargetInfo& target, int frameWindow,
                                             RecentFrameWindowMode mode);

/// 判断目标最新一帧是否有效
[[nodiscard]] bool latestFrameIsValid(const Core::TargetInfo& target);

/**
 * @brief 判断目标是否满足最近有效性规则
 * @param target 目标轨迹
 * @param frameWindow 有效性窗口大小
 * @param threshold 有效性阈值
 * @return 满足规则时返回 true
 */
[[nodiscard]] bool passesRecentValidityRule(const Core::TargetInfo& target, int frameWindow,
                                            float threshold);

/**
 * @brief 根据存活规则判断目标是否仍应保持活跃
 * @param target 目标轨迹
 * @param rule 存活判定规则
 * @return 目标仍存活时返回 true
 */
[[nodiscard]] bool targetRemainsLiving(const Core::TargetInfo& target, const TrackLivingRule& rule);

/// 用最新帧的有效状态更新目标的累积有效性指标
void updateValidityWithLatestFrame(Core::TargetInfo& target);

}  // namespace Dss::Tracking
