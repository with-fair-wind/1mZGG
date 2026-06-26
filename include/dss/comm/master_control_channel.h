#pragma once

#include <mutex>

#include "dss/comm/serial_command_interfaces.h"
#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"

namespace Dss::Comm {

/// 主控通道，接收主控指令帧并转发为系统事件
class MasterControlChannel final : public SerialWorkerBase, public IMasterControlStatusPort {
public:
    static constexpr auto Protocol = SerialProtocol::MasterControl;  ///< 本通道使用的协议类型

    /**
     * @brief 构造主控通道
     * @param bus 用于发布主控事件的消息总线
     */
    explicit MasterControlChannel(MessageBus& bus);

    /**
     * @brief 设置主控状态回包并请求发送
     * @param status 主控状态回包内容
     */
    void sendMasterControlStatus(const MasterControlStatus& status) override;

protected:
    /** @brief 获取主控协议接收帧长度。 @return 固定接收字节数。 */
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /** @brief 获取主控协议发送帧长度。 @return 固定发送字节数。 */
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /** @brief 获取主控通道诊断名称。 @return 协议布局中的稳定名称。 */
    [[nodiscard]] auto channelName() const -> std::string_view override {
        return layoutFor(Protocol).name;
    }

    /**
     * @brief 解码主控指令帧并发布主控事件。
     * @param data 已通过基础帧校验的接收字节。
     */
    void decodeFrame(std::span<const uint8_t> data) override;

    /**
     * @brief 使用当前缓存的主控状态编码发送帧。
     * @param buffer 固定长度的可写发送缓冲区。
     */
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    std::mutex m_statusMutex;               ///< 保护待发送主控状态的互斥锁
    MasterControlStatus m_pendingStatus{};  ///< 下一次发送使用的主控状态
};

}  // namespace Dss::Comm
