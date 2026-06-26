#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include "dss/acquisition/i_frame_source.h"

namespace Dss::Acquisition {

/// @brief 应用可选择的帧源模式。
enum class FrameSourceMode {
    Replay,  ///< 本地图像序列回放
    Live,    ///< 实时相机采集
};

/**
 * @brief 在实时采集与文件回放帧源之间进行线程安全切换。
 *
 * 协调器仅运行当前选中的帧源，并把统一的帧回调传播给所有已注册帧源。
 */
class FrameSourceCoordinator final : public IFrameSource {
public:
    /// @brief 停止所有帧源后销毁协调器。
    ~FrameSourceCoordinator() override;

    /**
     * @brief 注册指定模式的帧源。
     * @param mode 帧源模式。
     * @param source 帧源共享实例。
     * @return 注册成功时为空；实例为空或模式重复时返回错误描述。
     */
    auto registerSource(FrameSourceMode mode, std::shared_ptr<IFrameSource> source)
        -> std::expected<void, std::string>;

    /**
     * @brief 切换活动帧源，并保持原有运行状态。
     * @param mode 目标帧源模式。
     * @return 切换成功时为空；模式未注册或初始化失败时返回错误描述。
     */
    auto selectSource(FrameSourceMode mode) -> std::expected<void, std::string>;

    /**
     * @brief 获取当前活动模式。
     * @return 已选择的模式；尚无帧源时返回 std::nullopt。
     */
    [[nodiscard]] auto activeMode() const -> std::optional<FrameSourceMode>;

    auto init() -> std::expected<void, std::string> override;
    void start() override;
    void stop() override;
    void setFrameCallback(FrameCallback callback) override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] auto frameWidth() const -> uint32_t override;
    [[nodiscard]] auto frameHeight() const -> uint32_t override;

private:
    /**
     * @brief 在持有 m_mutex 时查找活动帧源。
     * @return 活动帧源共享指针；没有可用帧源时返回空指针。
     */
    [[nodiscard]] auto activeSourceLocked() const -> std::shared_ptr<IFrameSource>;

    mutable std::mutex m_mutex;  ///< 保护帧源表、活动模式和回调
    std::map<FrameSourceMode, std::shared_ptr<IFrameSource>> m_sources;  ///< 模式到帧源的映射
    std::optional<FrameSourceMode> m_activeMode;                         ///< 当前活动模式
    FrameCallback m_callback;                                            ///< 传播给各帧源的统一回调
};

}  // namespace Dss::Acquisition
