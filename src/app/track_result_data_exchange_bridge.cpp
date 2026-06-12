#include "dss/app/track_result_data_exchange_bridge.h"

#include <algorithm>
#include <utility>

#include "dss/core/result_packet_utils.h"

namespace Dss::App {

TrackResultDataExchangeBridge::TrackResultDataExchangeBridge(
    MessageBus& bus, GxtcSender sendGxtc, GdclSender sendGdcl,
    TrackResultDataExchangeBridgeOptions options)
    : m_options(std::move(options)),
      m_sendGxtc(std::move(sendGxtc)),
      m_sendGdcl(std::move(sendGdcl)) {
    m_connections.push_back(bus.subscribe<Dss::Core::MasterControlEvent>(
        [this](const Dss::Core::MasterControlEvent& event) { onMasterControl(event); }));
    m_connections.push_back(bus.subscribe<Dss::Core::TrackResultEvent>(
        [this](const Dss::Core::TrackResultEvent& event) { onTrackResult(event); }));
}

void TrackResultDataExchangeBridge::onMasterControl(
    const Dss::Core::MasterControlEvent& event) {
    std::lock_guard lock(m_mutex);
    m_masterControl = MasterControlState{
        .received = true,
        .track = event.track,
        .targetId = event.targetId,
    };
}

void TrackResultDataExchangeBridge::onTrackResult(const Dss::Core::TrackResultEvent& event) {
    auto packets = Dss::Core::makeResultPackets(event.targets);
    if (packets.empty()) {
        return;
    }

    const auto masterControl = masterControlState();
    auto options = mappingOptionsForPacket(packets.front(), masterControl, m_options.mapping);

    if (m_options.sendGxtc && m_sendGxtc) {
        auto targets = Dss::Network::makeGxtcTargets(packets, options);
        auto metadata = Dss::Network::makeGxtcMetadata(packets.front(), options);
        const auto hasValidTarget =
            std::ranges::any_of(packets, [](const Dss::Core::ResultPacket& packet) {
                return packet.valid;
            });
        metadata.targetStatus =
            hasValidTarget ? options.validTargetStatus : options.invalidTargetStatus;
        m_sendGxtc(metadata, targets);
    }

    if (!m_options.sendGdcl || !m_sendGdcl) {
        return;
    }

    for (const auto& packet : packets) {
        auto packetOptions = mappingOptionsForPacket(packet, masterControl, m_options.mapping);
        if (matchesMasterTarget(packet, masterControl, packetOptions)) {
            m_sendGdcl(Dss::Network::makeGdclMeasurement(packet, packetOptions));
        }
    }
}

auto TrackResultDataExchangeBridge::masterControlState() const -> MasterControlState {
    std::lock_guard lock(m_mutex);
    return m_masterControl;
}

auto TrackResultDataExchangeBridge::mappingOptionsForPacket(
    const Dss::Core::ResultPacket& packet, MasterControlState masterControl,
    Dss::Network::DataExchangeMappingOptions options)
    -> Dss::Network::DataExchangeMappingOptions {
    options.jms1970Centiseconds = Dss::Network::makeJms1970Centiseconds(packet.timestamp);
    if (masterControl.received) {
        options.measureStatus = masterControl.track ? 1U : 2U;
    }
    return options;
}

auto TrackResultDataExchangeBridge::matchesMasterTarget(
    const Dss::Core::ResultPacket& packet, MasterControlState masterControl,
    const Dss::Network::DataExchangeMappingOptions& options) -> bool {
    return masterControl.received && masterControl.targetId > 0U &&
           Dss::Network::parseLegacyTargetId(packet.targetId, options.fallbackTargetId) ==
               static_cast<int32_t>(masterControl.targetId);
}

}  // namespace Dss::App
