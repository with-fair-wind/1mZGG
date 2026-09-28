#pragma once

#include <cstddef>

#include "dss/core/types.h"

namespace Dss::Tracking {

/**
 * @brief 仅保留最新 maxFrames 帧，同步累计移除帧数，保留 validity 及其他算法状态。
 * @param target 待裁剪目标；调用方独占访问。
 * @param maxFrames 保留数量上限，必须大于零。
 * @throws std::invalid_argument maxFrames 为零。
 * @note 应在本帧预测、存活判定与候选去重完成后调用；调用者负责满足后续读取窗口。
 * 前端 erase 会使元素引用/迭代器失效；不逐帧 shrink_to_fit，容量不随轨迹时长增长。
 */
void retainRecentTargetFrames(Core::TargetInfo& target, std::size_t maxFrames);

/// 最近帧窗口检查模式
enum class RecentFrameWindowMode {
    RequireFullWindow,   ///< 要求帧数达到完整窗口才判定
    UseAvailableFrames,  ///< 使用当前已有的帧数进行判定
};

/// 目标丢失时的存活判定策略
enum class TrackMissPolicy {
    UseValidityWindow,                  ///< 基于有效性窗口与阈值判定
    RequireLatestValid,                 ///< 要求最新帧必须有效
    DropAfterConsecutiveInvalidFrames,  ///< 连续无效帧达到窗口上限则丢弃
};

/// 目标存活判定规则
struct TrackLivingRule {
    int frameWindow = 0;     ///< 检查的最近帧窗口大小
    float threshold = 0.0F;  ///< 有效性阈值（用于 UseValidityWindow）
    TrackMissPolicy missPolicy = TrackMissPolicy::UseValidityWindow;  ///< 丢失判定策略
};

/**
 * @brief 统计最近窗口内无效帧的数量
 * @param target 目标轨迹
 * @param frameWindow 检查窗口大小
 * @return 保留窗口内的无效帧计数。
 * @pre 若目标已裁剪，frameWindow 不得大于保留帧数；未裁剪短历史按已有帧统计。
 */
[[nodiscard]] auto countRecentInvalidFrames(const Core::TargetInfo& target, int frameWindow) -> int;

/**
 * @brief 判断最近窗口内的帧是否全部无效
 * @param target 目标轨迹
 * @param frameWindow 检查窗口大小
 * @param mode 窗口检查模式
 * @return 全部无效时返回 true
 * @pre 已裁剪目标的保留窗口必须覆盖 frameWindow；不推断被移除帧的有效性。
 */
[[nodiscard]] bool latestFramesAreAllInvalid(const Core::TargetInfo& target, int frameWindow,
                                             RecentFrameWindowMode mode);

/// 判断目标最新一帧是否有效
[[nodiscard]] bool latestFrameIsValid(const Core::TargetInfo& target);

/**
 * @brief 判断目标是否满足最近有效性规则
 * @param target 目标轨迹
 * @param frameWindow 开始判定的累计样本数门槛，不是 validity 的滚动平均窗口。
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

/**
 * @brief 用最新帧有效性和 totalFrameCount() 更新累计 validity，空历史不变。
 * @param target 已追加当前帧的目标；每次追加后调用一次，保留原初始化权重。
 * @note 与预测辅助函数使用相同累计样本语义，但保留各自原有浮点运算顺序。
 */
void updateValidityWithLatestFrame(Core::TargetInfo& target);

}  // namespace Dss::Tracking
