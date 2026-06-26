#pragma once

#include <mutex>

#include "dss/comm/serial_command_interfaces.h"
#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 伺服通道，将跟踪结果或联调参数编码为伺服修正帧并发送
class ServoChannel final : public SerialWorkerBase, public IServoCorrectionPort {
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

    /// @brief 设置伺服修正量并请求发送
    /// @param correction 伺服距离/速度修正参数
    void sendServoCorrection(const ServoCorrection& correction) override;

protected:
    /** @brief 获取伺服协议接收帧长度。 @return 固定接收字节数。 */
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /** @brief 获取伺服协议发送帧长度。 @return 固定发送字节数。 */
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /** @brief 获取伺服通道诊断名称。 @return 协议布局中的稳定名称。 */
    [[nodiscard]] auto channelName() const -> std::string_view override {
        return layoutFor(Protocol).name;
    }

    /**
     * @brief 解码伺服接收帧；当前接收数据未使用。
     * @param data 已通过基础帧校验的接收字节。
     */
    void decodeFrame(std::span<const uint8_t> data) override;

    /**
     * @brief 根据当前缓存的伺服修正量编码发送帧。
     * @param buffer 固定长度的可写发送缓冲区。
     */
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    std::mutex m_correctionMutex;           ///< 保护待发送伺服修正量的互斥锁
    ServoCorrection m_pendingCorrection{};  ///< 下一次发送使用的伺服修正量
};

}  // namespace Dss::Comm
