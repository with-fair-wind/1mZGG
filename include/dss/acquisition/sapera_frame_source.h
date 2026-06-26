#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>

#include "dss/acquisition/i_frame_source.h"
#include "dss/core/event_bus.h"

namespace Dss::Acquisition {

/// @brief Sapera 会话报告的二维帧尺寸。
struct SaperaFrameGeometry {
    uint32_t width = 0;   ///< 帧宽度，单位为像素
    uint32_t height = 0;  ///< 帧高度，单位为像素
};

/// @brief Sapera 采集设备及 CCF 文件配置。
struct SaperaConfig {
    std::string serverName{"Xtium-CL_MX4_1"};  ///< Sapera 服务器或采集卡名称
    int32_t deviceNumber = 0;                  ///< 服务器内的采集设备序号
    std::string ccfPath;                       ///< 相机 CCF 配置文件路径
};

/// @brief 隔离 Sapera SDK 生命周期和缓冲区回调的采集会话接口。
class ISaperaCaptureSession {
public:
    using FrameCallback =
        std::function<void(std::span<const uint16_t>)>;  ///< SDK 帧缓冲区就绪回调类型

    virtual ~ISaperaCaptureSession() = default;

    /**
     * @brief 初始化 SDK 对象并绑定帧回调。
     * @param callback 每个 16 位帧缓冲区就绪时调用的回调。
     * @return 成功时返回帧尺寸；失败时返回 SDK 错误描述。
     */
    virtual auto initialize(FrameCallback callback)
        -> std::expected<SaperaFrameGeometry, std::string> = 0;

    /**
     * @brief 启动连续采集。
     * @return 启动成功时为空；失败时返回错误描述。
     */
    virtual auto start() -> std::expected<void, std::string> = 0;

    /// @brief 停止连续采集。
    virtual void stop() = 0;

    /** @brief 查询采集状态。 @return 正在连续采集时返回 true。 */
    [[nodiscard]] virtual bool isRunning() const = 0;
};

/**
 * @brief 将 Sapera 16 位采集帧转换为应用 FramePacket 的实时帧源。
 */
class SaperaFrameSource final : public IFrameSource {
public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 跨线程消息总线类型

    /**
     * @brief 创建 Sapera 帧源。
     * @param config Sapera 设备配置。
     * @param session 可注入的采集会话；为空时初始化阶段创建 SDK 会话。
     * @param bus 用于发布采集错误的非拥有消息总线指针。
     */
    explicit SaperaFrameSource(SaperaConfig config = {},
                               std::unique_ptr<ISaperaCaptureSession> session = {},
                               MessageBus* bus = nullptr);

    /// @brief 停止采集并释放会话。
    ~SaperaFrameSource() override;

    SaperaFrameSource(const SaperaFrameSource&) = delete;
    SaperaFrameSource& operator=(const SaperaFrameSource&) = delete;

    auto init() -> std::expected<void, std::string> override;
    /**
     * @brief 启动已初始化的 Sapera 会话。
     * @return 启动成功时为空；未初始化或 SDK 启动失败时返回错误描述。
     */
    auto startCapture() -> std::expected<void, std::string>;
    void start() override;
    void stop() override;
    void setFrameCallback(FrameCallback callback) override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] auto frameWidth() const -> uint32_t override;
    [[nodiscard]] auto frameHeight() const -> uint32_t override;

private:
    /**
     * @brief 校验并复制一个 SDK 帧，然后调用应用帧回调。
     * @param pixels SDK 提供的 16 位像素视图，仅在本次调用期间有效。
     */
    void acceptFrame(std::span<const uint16_t> pixels);

    /**
     * @brief 发布 Sapera 采集错误事件。
     * @param message 错误描述。
     */
    void reportError(const std::string& message) const;

    SaperaConfig m_config;                             ///< Sapera 设备配置
    std::unique_ptr<ISaperaCaptureSession> m_session;  ///< 独占的采集会话
    MessageBus* m_bus = nullptr;                       ///< 非拥有错误事件总线指针

    mutable std::mutex m_mutex;  ///< 保护回调、尺寸、帧序号和初始化状态
    FrameCallback m_callback;    ///< 应用帧就绪回调
    uint32_t m_width = 0;        ///< 当前帧宽度
    uint32_t m_height = 0;       ///< 当前帧高度
    uint64_t m_frameSeq = 0;     ///< 下一帧序号
    bool m_initialized = false;  ///< 会话是否已成功初始化
};

}  // namespace Dss::Acquisition
