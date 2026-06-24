#pragma once

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"

namespace Dss::Comm {

/// 显示通道，接收显示协议帧并触发 25Hz 同步事件
class DisplayChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::Display;  ///< 本通道使用的协议类型

    /**
     * @brief 构造显示通道
     * @param bus 用于发布同步事件的消息总线
     */
    explicit DisplayChannel(MessageBus& bus);

protected:
    /** @brief 获取显示协议接收帧长度。 @return 固定接收字节数。 */
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /** @brief 获取显示协议发送帧长度。 @return 固定发送字节数。 */
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /** @brief 获取显示通道诊断名称。 @return 协议布局中的稳定名称。 */
    [[nodiscard]] auto channelName() const -> std::string_view override {
        return layoutFor(Protocol).name;
    }

    /**
     * @brief 解码显示帧，成功时发布 25 Hz 同步事件。
     * @param data 已通过基础帧校验的接收字节。
     */
    void decodeFrame(std::span<const uint8_t> data) override;

    /**
     * @brief 编码显示协议发送帧；当前协议没有有效载荷。
     * @param buffer 固定长度的可写发送缓冲区。
     */
    void encodeFrame(std::span<uint8_t> buffer) override;
};

}  // namespace Dss::Comm
