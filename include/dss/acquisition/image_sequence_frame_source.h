#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dss/acquisition/i_frame_source.h"

namespace Dss::Acquisition {

/// 从本地图像文件序列按序回放帧的帧源实现
class ImageSequenceFrameSource final : public IFrameSource {
public:
    ImageSequenceFrameSource();
    explicit ImageSequenceFrameSource(std::vector<std::filesystem::path> files);
    ~ImageSequenceFrameSource() override;

    /**
     * @brief 设置待回放的图像文件列表
     * @param files 图像文件路径序列
     * @return 列表为空时返回错误
     */
    auto setFiles(std::vector<std::filesystem::path> files) -> std::expected<void, std::string>;

    /// 当前序列中的帧总数
    [[nodiscard]] auto frameCount() const -> std::size_t;

    /// 下一帧待播放的索引
    [[nodiscard]] auto nextFrameIndex() const -> std::size_t;

    /// 设置连续回放时的帧间隔
    void setFrameInterval(std::chrono::milliseconds interval);

    /**
     * @brief 手动前进一帧并触发回调
     * @return 序列为空、未设置回调或已到末尾时返回错误
     */
    auto stepForward() -> std::expected<void, std::string>;

    /// 加载首帧确定尺寸并重置播放索引
    auto init() -> std::expected<void, std::string> override;

    /// 在后台线程中连续回放剩余帧
    void start() override;

    /// 停止回放并等待工作线程退出
    void stop() override;

    /// 设置帧就绪回调
    void setFrameCallback(FrameCallback callback) override;

    /// 是否正在连续回放
    [[nodiscard]] bool isRunning() const override;

    /// 帧宽度（像素）
    [[nodiscard]] auto frameWidth() const -> std::uint32_t override;

    /// 帧高度（像素）
    [[nodiscard]] auto frameHeight() const -> std::uint32_t override;

private:
    mutable std::mutex m_mutex;                  ///< 保护共享状态的互斥锁
    std::vector<std::filesystem::path> m_files;  ///< 待回放的图像文件路径列表
    FrameCallback m_callback;                    ///< 帧就绪回调
    std::chrono::milliseconds m_frameInterval{std::chrono::milliseconds{33}};  ///< 连续回放帧间隔
    std::uint32_t m_width = 0;                                                 ///< 帧宽度（像素）
    std::uint32_t m_height = 0;                                                ///< 帧高度（像素）
    std::size_t m_nextFrameIndex = 0;                                          ///< 下一帧待播放索引
    bool m_initialized = false;                                                ///< 是否已完成初始化

    std::jthread m_worker;               ///< 后台回放工作线程
    std::atomic<bool> m_running{false};  ///< 是否正在连续回放
};

}  // namespace Dss::Acquisition
