#pragma once

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"

namespace Dss::Comm {

/// 主控通道，接收主控指令帧并转发为系统事件
class MasterControlChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::MasterControl;  ///< 本通道使用的协议类型

    /**
     * @brief 构造主控通道
     * @param bus 用于发布主控事件的消息总线
     */
    explicit MasterControlChannel(MessageBus& bus);

protected:
    /// 返回主控协议接收帧长度
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /// 返回主控协议发送帧长度
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /// 解码主控指令帧并发布主控事件
    void decodeFrame(std::span<const uint8_t> data) override;

    /// 使用默认主控状态编码发送帧
    void encodeFrame(std::span<uint8_t> buffer) override;
};

}  // namespace Dss::Comm
