#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/network/data_exchange_protocol.h"

namespace Dss::App {

/// 跟踪结果到数据交换协议的桥接配置。
struct TrackResultDataExchangeBridgeOptions {
    Dss::Network::DataExchangeMappingOptions mapping{};  ///< 协议 DTO 映射选项
    bool sendGxtc = true;                                ///< 是否发送 GXTC 结果
    bool sendGdcl = true;                                ///< 是否发送 GDCL 结果
};

/// 将跟踪结果事件转换为 GXTC/GDCL 数据交换报文的桥接器。
class TrackResultDataExchangeBridge {
public:
    /// 应用内消息总线类型。
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;
    /// GXTC 发送回调类型。
    using GxtcSender =
        std::function<void(const Dss::Network::GxtcMetadata&,
                           std::span<const Dss::Network::GxtcTarget>)>;
    /// GDCL 发送回调类型。
    using GdclSender = std::function<void(const Dss::Network::GdclMeasurement&)>;

    /**
     * @brief 构造桥接器并订阅主控/跟踪事件
     * @param bus 应用消息总线
     * @param sendGxtc GXTC 发送回调
     * @param sendGdcl GDCL 发送回调
     * @param options 桥接配置
     */
    TrackResultDataExchangeBridge(MessageBus& bus, GxtcSender sendGxtc, GdclSender sendGdcl,
                                  TrackResultDataExchangeBridgeOptions options = {});

    ~TrackResultDataExchangeBridge() = default;

    TrackResultDataExchangeBridge(const TrackResultDataExchangeBridge&) = delete;
    auto operator=(const TrackResultDataExchangeBridge&)
        -> TrackResultDataExchangeBridge& = delete;
    TrackResultDataExchangeBridge(TrackResultDataExchangeBridge&&) = delete;
    auto operator=(TrackResultDataExchangeBridge&&) -> TrackResultDataExchangeBridge& = delete;

    /**
     * @brief 处理主控事件并缓存发送状态
     * @param event 主控事件
     */
    void onMasterControl(const Dss::Core::MasterControlEvent& event);

    /**
     * @brief 处理跟踪结果并触发 GXTC/GDCL 发送
     * @param event 跟踪结果事件
     */
    void onTrackResult(const Dss::Core::TrackResultEvent& event);

private:
    /// 主控状态缓存。
    struct MasterControlState {
        bool received = false;     ///< 是否收到过主控事件
        bool track = false;        ///< 主控是否处于搜索/跟踪状态
        uint32_t targetId = 0;     ///< 主控目标编号
    };

    /**
     * @brief 获取当前主控状态快照
     * @return 当前主控状态
     */
    [[nodiscard]] auto masterControlState() const -> MasterControlState;

    /**
     * @brief 根据结果包和主控状态生成协议映射选项
     * @param packet 通用测量结果包
     * @param masterControl 主控状态快照
     * @param options 基础映射选项
     * @return 填充时间与测量状态后的映射选项
     */
    [[nodiscard]] static auto mappingOptionsForPacket(
        const Dss::Core::ResultPacket& packet, MasterControlState masterControl,
        Dss::Network::DataExchangeMappingOptions options)
        -> Dss::Network::DataExchangeMappingOptions;

    /**
     * @brief 判断结果包是否匹配主控目标
     * @param packet 通用测量结果包
     * @param masterControl 主控状态快照
     * @param options 协议映射选项
     * @return 目标编号匹配时返回 true
     */
    [[nodiscard]] static auto matchesMasterTarget(
        const Dss::Core::ResultPacket& packet, MasterControlState masterControl,
        const Dss::Network::DataExchangeMappingOptions& options) -> bool;

    TrackResultDataExchangeBridgeOptions m_options{};  ///< 桥接配置
    GxtcSender m_sendGxtc;                             ///< GXTC 发送回调
    GdclSender m_sendGdcl;                             ///< GDCL 发送回调
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接
    mutable std::mutex m_mutex;                             ///< 主控状态互斥锁
    MasterControlState m_masterControl{};                   ///< 最近一次主控状态
};

}  // namespace Dss::App
