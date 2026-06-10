#pragma once

#include <mutex>

#include "dss/comm/serial_protocol_codec.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 曝光通道，接收曝光同步帧并缓存最新显示数据
class ExposureChannel final : public SerialWorkerBase {
public:
    static constexpr auto Protocol = SerialProtocol::Exposure;  ///< 本通道使用的协议类型

    /**
     * @brief 构造曝光通道
     * @param bus 用于发布曝光同步事件的消息总线
     */
    explicit ExposureChannel(MessageBus& bus);

    /// 获取最近一次解码成功的曝光显示数据
    [[nodiscard]] auto latestData() const -> Dss::Core::ExposureDisplayData;

protected:
    /// 返回曝光协议接收帧长度
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return layoutFor(Protocol).recvSize;
    }

    /// 返回曝光协议发送帧长度
    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return layoutFor(Protocol).sendSize;
    }

    /// 解码曝光帧，更新缓存并发布曝光同步事件
    void decodeFrame(std::span<const uint8_t> data) override;

    /// 使用默认曝光命令参数编码发送帧
    void encodeFrame(std::span<uint8_t> buffer) override;

private:
    mutable std::mutex m_dataMutex;                     ///< 保护最新数据的互斥锁
    Dss::Core::ExposureDisplayData m_latestData{};      ///< 最近一次解码成功的显示数据
};

}  // namespace Dss::Comm
