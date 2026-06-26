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
    /// @brief 创建尚未配置文件的回放帧源。
    ImageSequenceFrameSource();

    /**
     * @brief 使用文件序列创建回放帧源。
     * @param files 按播放顺序排列的图像文件路径。
     */
    explicit ImageSequenceFrameSource(std::vector<std::filesystem::path> files);

    /// @brief 停止并等待后台回放线程退出。
    ~ImageSequenceFrameSource() override;

    /**
     * @brief 设置待回放的图像文件列表
     * @param files 图像文件路径序列
     * @return 列表为空时返回错误
     */
    auto setFiles(std::vector<std::filesystem::path> files) -> std::expected<void, std::string>;

    /** @brief 获取当前序列帧数。 @return 已配置的图像文件数量。 */
    [[nodiscard]] auto frameCount() const -> std::size_t;

    /** @brief 获取下一帧播放索引。 @return 下一帧的零基索引。 */
    [[nodiscard]] auto nextFrameIndex() const -> std::size_t;

    /**
     * @brief 将下一帧定位到指定的零基索引
     * @param index 目标帧索引
     * @return 索引越界时返回错误
     */
    auto seek(std::size_t index) -> std::expected<void, std::string>;

    /**
     * @brief 设置连续回放时的帧间隔。
     * @param interval 相邻帧回调之间的目标间隔。
     */
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

    /**
     * @brief 设置帧就绪回调。
     * @param callback 每帧解码完成后调用的回调。
     */
    void setFrameCallback(FrameCallback callback) override;

    /** @brief 查询是否正在连续回放。 @return 后台回放线程运行时返回 true。 */
    [[nodiscard]] bool isRunning() const override;

    /** @brief 获取帧宽度。 @return 初始化得到的像素宽度。 */
    [[nodiscard]] auto frameWidth() const -> std::uint32_t override;

    /** @brief 获取帧高度。 @return 初始化得到的像素高度。 */
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
