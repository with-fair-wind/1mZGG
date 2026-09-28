#pragma once

#include <chrono>
#include <cstdint>

namespace Dss::Core {

/// 每个组件独立采样的载荷快照；共享缓冲可能跨组件重复计费，不能相加作为 RSS。
/// 字节数不含对象、分配器及未显式计费的算法临时内存；计数和峰值为实例生命周期累计。
struct ResourceSnapshot {
    std::uint64_t queuedItems = 0;           ///< 等待消费的项目数。
    std::uint64_t queuedBytes = 0;           ///< 等待载荷计费字节数。
    std::uint64_t activeItems = 0;           ///< 已取出且尚未完成的项目数。
    std::uint64_t activeBytes = 0;           ///< 正在处理的载荷计费字节数。
    std::uint64_t peakQueuedBytes = 0;       ///< 实例生命周期内等待载荷字节峰值。
    std::uint64_t completedItems = 0;        ///< 累计消费完成数，成功/失败由组件另行统计。
    std::uint64_t replacedItems = 0;         ///< 累计被新值替换的待处理项目数。
    std::uint64_t lastWorkMicroseconds = 0;  ///< 最近一次工作耗时，微秒。
    std::uint64_t maxWorkMicroseconds = 0;   ///< 实例生命周期内单次工作耗时峰值，微秒。
    std::uint64_t lastStopMicroseconds = 0;  ///< 最近一次停止/排空耗时，微秒。
};

/**
 * @brief 计算单调时钟起点至当前的非负时间间隔。
 * @param since 不晚于当前时刻的 steady_clock 起点。
 * @return 向下取整的微秒数。
 */
inline auto elapsedMicroseconds(std::chrono::steady_clock::time_point since) -> std::uint64_t {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now() - since)
                                          .count());
}

}  // namespace Dss::Core
