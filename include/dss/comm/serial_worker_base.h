#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
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

    /// 查询串口是否已打开
    [[nodiscard]] bool isOpen() const override;

    /// 获取当前通道运行状态
    [[nodiscard]] auto status() const -> Dss::Core::Status override;

    /// 返回最近一秒内的接收帧速率（帧/秒）
    [[nodiscard]] int recvFramesPerSec() const {
        return m_recvFps.load();
    }

    /// 返回最近一秒内的发送帧速率（帧/秒）
    [[nodiscard]] int sendFramesPerSec() const {
        return m_sendFps.load();
    }

protected:
    /// 返回本通道接收帧的固定字节长度
    [[nodiscard]] virtual auto recvFrameSize() const -> size_t override = 0;

    /// 返回本通道发送帧的固定字节长度
    [[nodiscard]] virtual auto sendFrameSize() const -> size_t override = 0;

    /// 解码接收到的原始帧数据
    virtual void decodeFrame(std::span<const uint8_t> data) = 0;

    /// 将待发送数据编码到缓冲区
    virtual void encodeFrame(std::span<uint8_t> buffer) = 0;

    /// 请求工作线程发送一帧
    void requestSend();

    /// 获取关联的消息总线引用
    MessageBus& bus() {
        return m_bus;
    }

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
