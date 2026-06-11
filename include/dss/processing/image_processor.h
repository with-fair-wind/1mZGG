#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "dss/core/event_bus.h"
#include "dss/processing/bounded_channel.h"
#include "dss/processing/frame_packet.h"
#include "dss/processing/i_processing_strategy.h"
#include "dss/processing/processing_pipeline.h"
#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Processing {

/// 图像处理器，在后台线程中异步处理帧并发布事件
class ImageProcessor {
public:
    /// 消息总线类型别名
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    /**
     * @brief 构造图像处理器
     * @param bus 事件消息总线引用
     */
    explicit ImageProcessor(MessageBus& bus);
    ~ImageProcessor();

    /// 启动后台处理线程
    void start();

    /// 停止后台处理线程并等待其退出
    void stop();

    /**
     * @brief 提交帧到处理队列
     * @param packet 待处理帧数据包
     * @return 队列已满时丢弃帧并返回 false
     */
    [[nodiscard]] bool submitFrame(FramePacket packet);

    /// 因队列满而被丢弃的帧计数
    [[nodiscard]] auto droppedFrames() const -> uint64_t;

    /// 后台处理线程是否正在运行
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief 设置图像处理策略
     * @param strategy 处理策略实例的所有权
     */
    void setProcessingStrategy(std::unique_ptr<IProcessingStrategy> strategy);

    /**
     * @brief 设置跟踪策略
     * @param strategy 跟踪策略实例的所有权
     */
    void setTrackingStrategy(std::unique_ptr<Dss::Tracking::ITrackingStrategy> strategy);

    /// 当前处理策略的模式
    [[nodiscard]] auto currentProcessingMode() const -> Dss::Core::ProcessingMode;

    /// 当前跟踪策略的模式；无策略时返回 TrackMode::Init
    [[nodiscard]] auto currentTrackMode() const -> Dss::Core::TrackMode;

private:
    /**
     * @brief 后台工作循环，从通道取帧并执行处理与跟踪
     * @param token 停止令牌，用于优雅退出
     */
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;                              ///< 事件消息总线
    BoundedChannel<FramePacket, 4> m_frameChannel;  ///< 帧输入有界通道
    std::jthread m_workerThread;                    ///< 后台处理线程
    std::atomic<bool> m_running{false};             ///< 运行状态标志
    std::atomic<uint64_t> m_droppedFrames{0};       ///< 丢弃帧计数

    mutable std::mutex m_strategyMutex;                                 ///< 保护策略对象的互斥锁
    ProcessingPipeline m_pipeline;                                      ///< 图像处理管线
    std::unique_ptr<Dss::Tracking::ITrackingStrategy> m_trackStrategy;  ///< 跟踪策略
};

}  // namespace Dss::Processing
