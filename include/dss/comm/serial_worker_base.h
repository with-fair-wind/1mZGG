#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "dss/comm/frame_codec.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/core/constants.h"
#include "dss/core/event_bus.h"

class QSerialPort;

namespace Dss::Comm {

/// 串口工作线程基类，在独立线程中完成帧收发与编解码
class SerialWorkerBase : public ISerialChannel {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造串口工作线程基类
     * @param bus 用于发布通信事件的消息总线
     */
    explicit SerialWorkerBase(MessageBus& bus);
    /// 析构并关闭串口
    ~SerialWorkerBase() override;

    SerialWorkerBase(const SerialWorkerBase&) = delete;
    SerialWorkerBase& operator=(const SerialWorkerBase&) = delete;

    /**
     * @brief 打开串口并启动收发工作线程
     * @param config 串口配置（端口名、波特率等）
     * @return 成功返回空值，失败返回错误描述
     */
    auto open(const SerialConfig& config) -> std::expected<void, std::string> override;

    /// 关闭串口并停止工作线程
    void close() override;

    /** @brief 查询串口是否已打开。 @return Qt 串口处于打开状态时返回 true。 */
    [[nodiscard]] bool isOpen() const override;

    /** @brief 获取当前通道状态。 @return 原子读取的生命周期状态。 */
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /**
     * @brief 获取最近统计周期的接收帧速率。
     * @return 每秒接收帧数。
     */
    [[nodiscard]] int recvFramesPerSec() const {
        return m_recvFps.load();
    }

    /**
     * @brief 获取最近统计周期的发送帧速率。
     * @return 每秒发送帧数。
     */
    [[nodiscard]] int sendFramesPerSec() const {
        return m_sendFps.load();
    }

protected:
    /** @brief 获取本通道接收帧长度。 @return 协议定义的固定字节数。 */
    [[nodiscard]] virtual auto recvFrameSize() const -> size_t override = 0;

    /** @brief 获取本通道发送帧长度。 @return 协议定义的固定字节数。 */
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t override = 0;

    /**
     * @brief 解码接收到的完整原始帧。
     * @param data 已通过帧头、帧尾和长度校验的字节视图。
     */
    virtual void decodeFrame(std::span<const uint8_t> data) = 0;

    /**
     * @brief 将待发送状态编码到固定长度缓冲区。
     * @param buffer 由工作线程分配的可写发送缓冲区。
     */
    virtual void encodeFrame(std::span<uint8_t> buffer) = 0;

    /**
     * @brief 获取用于日志和诊断事件的通道名称。
     * @return 稳定的通道名称字符串视图。
     */
    [[nodiscard]] virtual auto channelName() const -> std::string_view {
        return "serial";
    }

    /// 请求工作线程发送一帧
    void requestSend();

    /** @brief 获取关联的消息总线。 @return 构造时绑定的消息总线引用。 */
    MessageBus& bus() {
        return m_bus;
    }

    /**
     * @brief 发布协议字段级解码失败事件。
     * @param field 解码失败的协议字段名称。
     * @param message 错误描述。
     * @param byteOffset 字段在帧中的起始字节偏移。
     * @param rawValue 触发错误的原始字段值。
     */
    void publishDecodeError(std::string_view field, std::string_view message,
                            std::size_t byteOffset, uint64_t rawValue);

private:
    /**
     * @brief 工作线程主循环：轮询接收、处理发送请求并统计帧速率
     * @param token 线程停止令牌
     */
    void workerLoop(std::stop_token token);

    /// 从串口读取完整帧，校验后调用 decodeFrame
    void onDataReceived();

    /// 编码一帧并通过串口发送
    void sendFrameInternal();

    MessageBus& m_bus;                          ///< 事件总线引用
    SerialConfig m_config{};                    ///< 当前串口配置
    std::unique_ptr<QSerialPort> m_serialPort;  ///< Qt 串口对象
    std::jthread m_workerThread;                ///< 收发工作线程

    std::mutex m_sendMutex;        ///< 发送请求互斥锁
    bool m_sendRequested = false;  ///< 是否有待发送帧

    std::atomic<Dss::Core::Status> m_status{Dss::Core::Status::Init};  ///< 通道运行状态
    std::atomic<int> m_recvFps{0};                                     ///< 接收帧速率（帧/秒）
    std::atomic<int> m_sendFps{0};                                     ///< 发送帧速率（帧/秒）
    std::atomic<int> m_recvCount{0};                                   ///< 当前统计周期内接收帧计数
    std::atomic<int> m_sendCount{0};                                   ///< 当前统计周期内发送帧计数
};

}  // namespace Dss::Comm
