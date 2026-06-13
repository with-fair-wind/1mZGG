#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/udp_channel.h"

namespace Dss::Network {

/// 图像 UDP 发送服务，将图像分片编码后通过后台线程异步发送
class ImageSender : public INetworkChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    static constexpr std::size_t MaxUdpPayload = 60U * 1024U;  ///< 单个 UDP 分片最大载荷（字节）
    static constexpr std::size_t PacketHeaderSize = 20U;       ///< 分片包头长度（字节）
    static constexpr std::size_t ImageHeaderSize = 10U;        ///< 图像编码头长度（字节）
    static constexpr std::size_t PacketPaddingSize = 4U;       ///< 分片尾部填充长度（字节）

    /**
     * @brief 构造图像发送服务
     * @param bus 事件总线引用，发送完成后发布 ImageSendEvent
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

    /// 查询通道是否已绑定
    [[nodiscard]] bool isOpen() const override;

    /// 获取当前网络通道运行状态
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /**
     * @brief 提交待发送图像（异步，由工作线程分片发送）
     * @param imageData 原始像素数据
     * @param width 图像宽度（像素）
     * @param height 图像高度（像素）
     */
    void sendImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height);

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

    MessageBus& m_bus;            ///< 事件总线引用
    UdpChannel m_channel;         ///< 图像 UDP 通道
    std::jthread m_workerThread;  ///< 异步发送工作线程

    std::mutex m_bufferMutex;                ///< 保护待发送缓冲区的互斥锁
    std::condition_variable_any m_bufferCv;  ///< 待发送图像就绪条件变量
    std::vector<uint8_t> m_pendingImage;     ///< 待发送像素数据
    uint32_t m_pendingWidth = 0;             ///< 待发送图像宽度
    uint32_t m_pendingHeight = 0;            ///< 待发送图像高度
    bool m_hasPending = false;               ///< 是否有待发送图像
};

}  // namespace Dss::Network
