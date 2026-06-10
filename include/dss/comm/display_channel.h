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
    /// 返回显示协议接收帧长度
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /// 返回显示协议发送帧长度
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /// 解码显示帧，成功时发布 25Hz 同步事件
    void decodeFrame(std::span<const uint8_t> data) override;

    /// 编码显示协议发送帧（当前无有效载荷）
    void encodeFrame(std::span<uint8_t> buffer) override;
};

}  // namespace Dss::Comm
