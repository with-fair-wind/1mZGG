#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace Dss::Storage {

/// @brief 轨迹 writer 的串行阶段；Path 包括会话锁等待与路径复制。
enum class TrackWriterPhase : std::uint8_t {
    Path,       ///< 获取会话路径，包括锁等待和路径复制。
    Directory,  ///< 打开失败后的目录修复。
    Open,       ///< 文件打开及必要的重试。
    Format,     ///< 一个批次内各记录的格式化总和。
    Append,     ///< 向流追加文本，包括可能触发的缓冲写入。
    Close,      ///< 关闭文件及其缓冲刷新。
    Count,      ///< 阶段数量，不作为计时阶段。
};

/// @brief 固定大小的 writer 阶段统计，单位为纳秒，不包含排队等待。
struct TrackWriterProfileSnapshot {
    std::uint64_t batches = 0;  ///< 已结束的写入尝试，包括失败。
    std::array<std::uint64_t, static_cast<std::size_t>(TrackWriterPhase::Count)>
        total{};  ///< 各阶段累计时间。
    std::array<std::uint64_t, static_cast<std::size_t>(TrackWriterPhase::Count)>
        maximum{};  ///< 各阶段单批最大时间。
};

/// @brief 可选的固定容量计时器；单 writer 更新，任意线程可读，无载荷或历史列表。
/// @note 运行中快照各字段不保证来自同一时刻；stop 后可读取一致的最终统计。
/// 阶段为墙钟间隔，包含调度和计时开销；异常时的资源清理计入当时阶段，不是 CPU 采样。
class TrackWriterProfile {
public:
    /// @brief 一个批次的阶段计时，析构时即使异常也记录已执行阶段。
    class Batch {
    public:
        /// @brief 开始计时；空指针关闭时钟读取和记录。
        /// @param profile 非拥有计时器，须存活至批次析构；可为空。
        explicit Batch(TrackWriterProfile* profile) : m_profile(profile) {
            if (m_profile != nullptr) {
                m_previous = Clock::now();
            }
        }
        Batch(const Batch&) = delete;
        auto operator=(const Batch&) -> Batch& = delete;
        Batch(Batch&&) = delete;
        auto operator=(Batch&&) -> Batch& = delete;
        /// @brief 结算最后阶段；固定大小原子计数不分配内存或调用用户回调。
        ~Batch() {
            if (m_profile != nullptr) {
                begin(m_phase);
                m_profile->record(m_elapsed);
            }
        }
        /// @brief 结束上一阶段并进入下一阶段；同一阶段可反复累计。
        /// @param phase 接下来执行的阶段。
        /// @pre phase 为有效阶段，不能为 Count。
        void begin(TrackWriterPhase phase) {
            if (m_profile == nullptr) {
                return;
            }
            const auto now = Clock::now();
            m_elapsed[static_cast<std::size_t>(m_phase)] += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_previous).count());
            m_previous = now;
            m_phase = phase;
        }

    private:
        using Clock = std::chrono::steady_clock;            ///< 单调计时时钟。
        TrackWriterProfile* m_profile;                      ///< 非拥有，可为空。
        Clock::time_point m_previous;                       ///< 上一计时边界。
        TrackWriterPhase m_phase = TrackWriterPhase::Path;  ///< 当前阶段。
        std::array<std::uint64_t, static_cast<std::size_t>(TrackWriterPhase::Count)>
            m_elapsed{};  ///< 当前批次。
    };

    /// @brief 读取累计统计；不改变或清空计数。
    /// @return 运行中近似快照，停止后为稳定最终值。
    [[nodiscard]] auto snapshot() const -> TrackWriterProfileSnapshot {
        TrackWriterProfileSnapshot result;
        result.batches = m_batches.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < result.total.size(); ++index) {
            result.total[index] = m_total[index].load(std::memory_order_relaxed);
            result.maximum[index] = m_maximum[index].load(std::memory_order_relaxed);
        }
        return result;
    }

private:
    /// @brief 累计一个已结束批次；同一计时器仅由一个 writer 调用。
    /// @param elapsed 本批各阶段纳秒数。
    void record(const std::array<std::uint64_t, static_cast<std::size_t>(TrackWriterPhase::Count)>&
                    elapsed) {
        for (std::size_t index = 0; index < elapsed.size(); ++index) {
            m_total[index].fetch_add(elapsed[index], std::memory_order_relaxed);
            if (elapsed[index] > m_maximum[index].load(std::memory_order_relaxed)) {
                m_maximum[index].store(elapsed[index], std::memory_order_relaxed);
            }
        }
        m_batches.fetch_add(1, std::memory_order_relaxed);
    }
    std::atomic<std::uint64_t> m_batches{0};  ///< 写入尝试数。
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(TrackWriterPhase::Count)>
        m_total{};  ///< 累计时间。
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(TrackWriterPhase::Count)>
        m_maximum{};  ///< 单批最大时间。
};
}  // namespace Dss::Storage
