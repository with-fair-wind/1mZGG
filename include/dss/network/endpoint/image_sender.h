#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

#include "dss/core/diagnostics/resource_snapshot.h"
#include "dss/core/event/message_bus.h"
#include "dss/network/transport/i_network_channel.h"
#include "dss/network/transport/udp_channel.h"

namespace Dss::Network {

/// 图像 UDP 发送服务，将图像分片编码后通过后台线程异步发送
class ImageSender : public INetworkChannel {
public:
    using MessageBus = Dss::Core::MessageBus;  ///< 事件总线类型别名

    /// 在发送线程按需生成拥有像素的 8 位图像。
    using ImageFactory = std::function<std::shared_ptr<const std::vector<std::uint8_t>>()>;

    static constexpr std::size_t MaxUdpPayload = 60U * 1024U;  ///< 单个 UDP 分片最大载荷（字节）
    static constexpr std::size_t PacketHeaderSize = 20U;       ///< 分片包头长度（字节）
    static constexpr std::size_t ImageHeaderSize = 10U;        ///< 图像编码头长度（字节）
    static constexpr std::size_t PacketPaddingSize = 4U;       ///< 分片尾部填充长度（字节）

    /**
     * @brief 构造图像发送服务
     * @param bus 事件总线引用，发送完成后发布 ImageSendCompletedEvent
     */
    explicit ImageSender(MessageBus& bus);

    /// 析构时自动关闭通道并停止工作线程
    ~ImageSender() override;

    /**
     * @brief 绑定 UDP 端点并启动发送工作线程
     * @param config UDP 端点配置
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string> override;

    /// 停止工作线程并关闭 UDP 通道
    void close() override;

    /** @brief 查询图像发送通道是否已绑定。 @return UDP 通道可发送时返回 true。 */
    [[nodiscard]] bool isOpen() const override;

    /** @brief 获取图像发送服务状态。 @return 已绑定时为 Running，否则为 Init。 */
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /**
     * @brief 提交待发送图像（异步，由工作线程分片发送）
     * @param imageData 原始像素数据
     * @param width 图像宽度（像素）
     * @param height 图像高度（像素）
     */
    void sendImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height);

    /**
     * @brief 提交带帧序号的共享图像缓冲（不复制像素数据）。
     * @param frameSeq 图像帧序号。
     * @param imageData 不可变 8 位灰度图像共享缓冲。
     * @param width 图像宽度（像素）。
     * @param height 图像高度（像素）。
     */
    void sendImage(uint64_t frameSeq, std::shared_ptr<const std::vector<uint8_t>> imageData,
                   uint32_t width, uint32_t height);

    /**
     * @brief 提交带延迟生成器的待发送帧(仅更新单槽,由工作线程按需调用 factory)。
     * @param frameSeq 图像帧序号。
     * @param image 已就绪的 8 位图像;为空时由 imageFactory 在工作线程生成。
     * @param imageFactory 延迟 8 位图生成器;image 非空时可留空。
     * @param retainedSourceBytes 延迟工厂捕获的源载荷字节数，不与已持有图像重复计量。
     * @param width 图像宽度(像素)。
     * @param height 图像高度(像素)。
     */
    void submitForSend(uint64_t frameSeq, std::shared_ptr<const std::vector<uint8_t>> image,
                       ImageFactory imageFactory, uint32_t width, uint32_t height,
                       std::size_t retainedSourceBytes = 0);

    /** @brief 获取本组件的资源采样。 @return 发送等待槽与活动帧的资源计数。 */
    [[nodiscard]] auto resourceSnapshot() const -> Dss::Core::ResourceSnapshot;

    /**
     * @brief 将图像编码并拆分为 UDP 分片列表
     * @param imageData 原始像素数据
     * @param width 图像宽度（像素）
     * @param height 图像高度（像素）
     * @return 分片列表；数据无效或过大时返回空列表
     */
    [[nodiscard]] static auto buildPackets(std::span<const uint8_t> imageData, uint32_t width,
                                           uint32_t height) -> std::vector<std::vector<uint8_t>>;

private:
    /**
     * @brief 工作线程主循环，等待待发送图像并逐片发送
     * @param token 停止令牌，用于优雅退出
     */
    void workerLoop(std::stop_token token);

    /// @brief 在持有生命周期互斥锁时停止线程并关闭通道。
    void closeLocked();

    MessageBus& m_bus;            ///< 事件总线引用
    UdpChannel m_channel;         ///< 图像 UDP 通道
    std::jthread m_workerThread;  ///< 异步发送工作线程
    std::mutex m_lifecycleMutex;  ///< 串行化通道启停操作

    mutable std::mutex m_bufferMutex;                            ///< 保护待发送缓冲区的互斥锁
    std::condition_variable_any m_bufferCv;                      ///< 待发送图像就绪条件变量
    std::shared_ptr<const std::vector<uint8_t>> m_pendingImage;  ///< 待发送共享像素数据
    ImageFactory m_pendingImageFactory;                          ///< 待发送图像的延迟生成器
    uint64_t m_pendingFrameSeq = 0;                              ///< 待发送帧序号
    uint32_t m_pendingWidth = 0;                                 ///< 待发送图像宽度
    uint32_t m_pendingHeight = 0;                                ///< 待发送图像高度
    Dss::Core::ResourceSnapshot m_resources;                     ///< bufferMutex 保护。
    bool m_hasPending = false;                                   ///< 是否有待发送图像
    bool m_accepting = false;                                    ///< 是否接受新的发送请求
};

}  // namespace Dss::Network
