#include "dss/ui/network_endpoint_helpers.h"

#include <cstdint>

namespace Dss::Ui {
namespace {

/// UDP 端口最大值。
constexpr int kMaxUdpPort = 65535;

}  // namespace

auto endpointLocalIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString {
    return QString::fromStdString(endpoint.localIp);
}

auto endpointRemoteIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString {
    return QString::fromStdString(endpoint.remoteIp);
}

auto isLocalUdpPortInRange(int port) -> bool {
    return port >= 0 && port <= kMaxUdpPort;
}

auto isRemoteUdpPortInRange(int port) -> bool {
    return port > 0 && port <= kMaxUdpPort;
}

auto makeUdpEndpointConfig(const QString& localIp, int localPort, const QString& remoteIp,
                           int remotePort) -> Dss::Core::UdpEndpointConfig {
    Dss::Core::UdpEndpointConfig endpoint{};
    endpoint.localIp = localIp.trimmed().toStdString();
    endpoint.localPort = static_cast<std::uint16_t>(localPort);
    endpoint.remoteIp = remoteIp.trimmed().toStdString();
    endpoint.remotePort = static_cast<std::uint16_t>(remotePort);
    return endpoint;
}

}  // namespace Dss::Ui
