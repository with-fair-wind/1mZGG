#pragma once

#include <mutex>

#include "dss/comm/serial_command_interfaces.h"
#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 曝光通道，接收曝光同步帧并缓存最新显示数据
class ExposureChannel final : public SerialWorkerBase, public IExposureCommandPort {
public:
    static constexpr auto Protocol = SerialProtocol::Exposure;  ///< 本通道使用的协议类型

    /**
     * @brief 构造曝光通道
     * @param bus 用于发布曝光同步事件的消息总线
     */
    explicit ExposureChannel(MessageBus& bus);

    /// 获取最近一次解码成功的曝光显示数据
    [[nodiscard]] auto latestData() const -> Dss::Core::ExposureDisplayData;

    /**
     * @brief 设置曝光发送命令并请求发送
     * @param command 曝光通道发送命令参数
     */
    void sendExposureCommand(const ExposureCommand& command) override;

protected:
    /// 返回曝光协议接收帧长度
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /// 返回曝光协议发送帧长度
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /// 返回曝光通道诊断名称
    [[nodiscard]] auto channelName() const -> std::string_view override {
        return layoutFor(Protocol).name;
    }

    /// 解码曝光帧，更新缓存并发布曝光同步事件
    void decodeFrame(std::span<const uint8_t> data) override;

    /// 使用当前缓存的曝光命令参数编码发送帧
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    mutable std::mutex m_dataMutex;                 ///< 保护最新数据的互斥锁
    Dss::Core::ExposureDisplayData m_latestData{};  ///< 最近一次解码成功的显示数据
    std::mutex m_commandMutex;                      ///< 保护待发送曝光命令的互斥锁
    ExposureCommand m_pendingCommand{};             ///< 下一次发送使用的曝光命令
};

}  // namespace Dss::Comm
