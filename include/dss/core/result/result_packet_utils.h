#pragma once

#include <optional>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Core {

/// 获取目标轨迹中的最新帧信息。
///
/// @param target 待读取的目标轨迹。
/// @return 有帧信息时返回最新帧指针；轨迹为空时返回 nullptr。
[[nodiscard]] auto latestTargetFrameInfo(const TargetInfo& target) -> const TargetFrameInfo*;

/// 将目标轨迹最新帧归一为测量结果数据包。
///
/// @param target 待转换的目标轨迹。
/// @return 轨迹有帧信息时返回结果数据包；轨迹为空时返回空。
[[nodiscard]] auto makeResultPacket(const TargetInfo& target) -> std::optional<ResultPacket>;

/// 批量构造测量结果数据包。
///
/// @param targets 待转换的目标轨迹列表。
/// @return 跳过空轨迹后的结果数据包列表。
[[nodiscard]] auto makeResultPackets(std::span<const TargetInfo> targets)
    -> std::vector<ResultPacket>;

}  // namespace Dss::Core
