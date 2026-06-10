#pragma once

#include <mutex>

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 伺服通道，将跟踪结果编码为伺服修正帧并发送
class ServoChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::Servo;  ///< 本通道使用的协议类型

    /**
     * @brief 构造伺服通道
     * @param bus 消息总线（本通道主要使用发送路径）
     */
    explicit ServoChannel(MessageBus& bus);

    /**
     * @brief 设置跟踪结果并触发发送
     * @param target 当前跟踪目标信息
     */
    void setTrackResult(const Dss::Core::TargetInfo& target);

protected:
    /// 返回伺服协议接收帧长度
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /// 返回伺服协议发送帧长度
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /// 解码伺服接收帧（当前未使用）
    void decodeFrame(std::span<const uint8_t> data) override;

    /// 根据当前跟踪目标生成伺服修正量并编码到发送帧
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    std::mutex m_targetMutex;                   ///< 保护当前目标的互斥锁
    Dss::Core::TargetInfo m_currentTarget{};    ///< 当前跟踪目标信息
};

}  // namespace Dss::Comm
