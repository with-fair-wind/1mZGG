#include "dss/ui/network_view_model.h"

#include <array>
#include <expected>
#include <memory>
#include <string_view>

#include "dss/core/config.h"
#include "dss/network/i_network_channel.h"
#include "dss/ui/network_endpoint_helpers.h"

namespace Dss::Ui {
namespace {

/// @brief CommNetConfig 中 UDP 端点成员指针类型。
using EndpointMember = Dss::Core::UdpEndpointConfig Dss::Core::CommNetConfig::*;

/// @brief 可编辑网络端点描述。
struct NetworkEndpointDescriptor {
    std::string_view key;          ///< 端点键名。
    std::string_view displayName;  ///< 界面显示名称。
    EndpointMember member;         ///< 对应配置成员。
};

/// @brief 可由 UI 显式打开/关闭的网络服务描述。
struct NetworkServiceDescriptor {
    std::string_view key;          ///< 端点键名。
    std::string_view serviceName;  ///< 服务注册名。
    std::string_view displayName;  ///< 界面显示名称。
    EndpointMember member;         ///< 对应配置成员。
};

/// 可由 UI 编辑的 UDP 端点描述表。
constexpr std::array kNetworkEndpointDescriptors{
    NetworkEndpointDescriptor{"image_sender", "Image Sender",
                              &Dss::Core::CommNetConfig::imageSender},
    NetworkEndpointDescriptor{"error_diag", "Error Diagnostics",
                              &Dss::Core::CommNetConfig::errorDiag},
    NetworkEndpointDescriptor{"atmos", "Atmos Receiver", &Dss::Core::CommNetConfig::atmos},
    NetworkEndpointDescriptor{"heartbeat", "Heartbeat", &Dss::Core::CommNetConfig::heartbeat},
    NetworkEndpointDescriptor{"exchange_gxtc", "GXTC Data Exchange",
                              &Dss::Core::CommNetConfig::exchangeGxtc},
    NetworkEndpointDescriptor{"exchange_gdcl", "GDCL Data Exchange",
                              &Dss::Core::CommNetConfig::exchangeGdcl},
};

/// 可由 UI 显式打开/关闭的网络服务描述表。
constexpr std::array kNetworkServiceDescriptors{
    NetworkServiceDescriptor{"image_sender", "image_sender", "Image Sender",
                             &Dss::Core::CommNetConfig::imageSender},
    NetworkServiceDescriptor{"error_diag", "error_diagnostics", "Error Diagnostics",
                             &Dss::Core::CommNetConfig::errorDiag},
    NetworkServiceDescriptor{"atmos", "atmos_receiver", "Atmos Receiver",
                             &Dss::Core::CommNetConfig::atmos},
    NetworkServiceDescriptor{"heartbeat", "heartbeat", "Heartbeat",
                             &Dss::Core::CommNetConfig::heartbeat},
};

/// @brief 查找网络端点描述。
[[nodiscard]] auto findNetworkEndpointDescriptor(const QString& key)
    -> const NetworkEndpointDescriptor* {
    const auto normalizedKey = key.trimmed().toStdString();
    for (const auto& descriptor : kNetworkEndpointDescriptors) {
        if (descriptor.key == normalizedKey) {
            return &descriptor;
        }
    }
    return nullptr;
}

/// @brief 查找可控网络服务描述。
[[nodiscard]] auto findNetworkServiceDescriptor(const QString& key)
    -> const NetworkServiceDescriptor* {
    const auto normalizedKey = key.trimmed().toStdString();
    for (const auto& descriptor : kNetworkServiceDescriptors) {
        if (descriptor.key == normalizedKey) {
            return &descriptor;
        }
    }
    return nullptr;
}

/// @brief 将描述表文本转为 Qt 字符串。
[[nodiscard]] auto descriptorText(std::string_view text) -> QString {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// @brief 获取已注册的统一网络通道服务。
[[nodiscard]] auto registeredNetworkService(Dss::Core::ServiceRegistry& registry,
                                            std::string_view name)
    -> std::shared_ptr<Dss::Network::INetworkChannel> {
    return registry.tryGet<Dss::Network::INetworkChannel>(name);
}

/// @brief 打开已注册的统一网络通道服务。
[[nodiscard]] auto openRegisteredNetworkService(Dss::Core::ServiceRegistry& registry,
                                                std::string_view name,
                                                const Dss::Core::UdpEndpointConfig& config)
    -> std::expected<void, std::string> {
    auto service = registeredNetworkService(registry, name);
    if (!service) {
        return std::unexpected("service is not registered");
    }
    return service->open(config);
}

/// @brief 关闭已注册的统一网络通道服务。
[[nodiscard]] auto closeRegisteredNetworkService(Dss::Core::ServiceRegistry& registry,
                                                 std::string_view name) -> bool {
    auto service = registeredNetworkService(registry, name);
    if (!service) {
        return false;
    }
    service->close();
    return true;
}

/// @brief 判断端点是否有对应的可控网络服务。
[[nodiscard]] auto hasControllableService(std::string_view key) -> bool {
    for (const auto& descriptor : kNetworkServiceDescriptors) {
        if (descriptor.key == key) {
            return true;
        }
    }
    return false;
}

/// @brief 将端点配置转换为 UI 状态。
[[nodiscard]] auto makeNetworkEndpointState(const NetworkEndpointDescriptor& descriptor,
                                            const Dss::Core::UdpEndpointConfig& endpoint,
                                            bool isOpen) -> NetworkEndpointState {
    return NetworkEndpointState{
        .key = descriptorText(descriptor.key),
        .displayName = descriptorText(descriptor.displayName),
        .localIp = endpointLocalIp(endpoint),
        .localPort = endpoint.localPort,
        .remoteIp = endpointRemoteIp(endpoint),
        .remotePort = endpoint.remotePort,
        .canOpen = hasControllableService(descriptor.key),
        .isOpen = isOpen,
    };
}

/// @brief 判断端点描述是否属于 GXTC/GDCL 数据交换端点。
[[nodiscard]] auto isDataExchangeEndpoint(const NetworkEndpointDescriptor& descriptor) -> bool {
    return descriptor.member == &Dss::Core::CommNetConfig::exchangeGxtc ||
           descriptor.member == &Dss::Core::CommNetConfig::exchangeGdcl;
}

}  // namespace

NetworkViewModel::NetworkViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

NetworkViewModel::~NetworkViewModel() = default;

auto NetworkViewModel::networkEndpointConfigs() -> std::vector<NetworkEndpointState> {
    const auto& commNet = Dss::Core::Config::instance().commNet();
    std::vector<NetworkEndpointState> endpoints;
    endpoints.reserve(kNetworkEndpointDescriptors.size());
    for (const auto& descriptor : kNetworkEndpointDescriptors) {
        endpoints.push_back(
            makeNetworkEndpointState(descriptor, commNet.*(descriptor.member),
                                     isNetworkEndpointOpen(descriptorText(descriptor.key))));
    }
    return endpoints;
}

bool NetworkViewModel::isNetworkEndpointOpen(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        return false;
    }

    const auto service = registeredNetworkService(m_registry, descriptor->serviceName);
    return service && service->isOpen();
}

bool NetworkViewModel::applyNetworkEndpointConfig(const QString& key, const QString& localIp,
                                                  int localPort, const QString& remoteIp,
                                                  int remotePort) {
    const auto* descriptor = findNetworkEndpointDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown network endpoint: %1").arg(key));
        return false;
    }
    if (!isLocalUdpPortInRange(localPort)) {
        Q_EMIT statusTextChanged("Network endpoint local port must be 0-65535");
        return false;
    }
    if (!isRemoteUdpPortInRange(remotePort)) {
        Q_EMIT statusTextChanged("Network endpoint remote port must be 1-65535");
        return false;
    }

    auto& commNet = Dss::Core::Config::instance().mutableCommNet();
    commNet.*(descriptor->member) = makeUdpEndpointConfig(localIp, localPort, remoteIp, remotePort);
    if (descriptor->member == &Dss::Core::CommNetConfig::exchangeGxtc) {
        commNet.exchange = commNet.exchangeGxtc;
    }

    if (isDataExchangeEndpoint(*descriptor)) {
        Q_EMIT dataExchangeEndpointEdited();
        Q_EMIT dataExchangeEndpointsChanged();
    }
    if (findNetworkServiceDescriptor(key) != nullptr && isNetworkEndpointOpen(key)) {
        closeNetworkEndpoint(key);
    }

    Q_EMIT networkEndpointsChanged();
    Q_EMIT statusTextChanged(
        QString("Network endpoint applied: %1").arg(descriptorText(descriptor->displayName)));
    return true;
}

bool NetworkViewModel::openNetworkEndpoint(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown network service: %1").arg(key));
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();
    const auto& endpoint = commNet.*(descriptor->member);
    auto result = openRegisteredNetworkService(m_registry, descriptor->serviceName, endpoint);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!result.has_value()) {
        if (result.error() == "service is not registered") {
            Q_EMIT statusTextChanged(
                QString("Network service is not registered: %1").arg(displayName));
        } else {
            Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        }
        Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), false);
        Q_EMIT networkEndpointsChanged();
        return false;
    }

    Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), true);
    Q_EMIT networkEndpointsChanged();
    Q_EMIT statusTextChanged(QString("Network service opened: %1").arg(displayName));
    return true;
}

void NetworkViewModel::closeNetworkEndpoint(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown network service: %1").arg(key));
        return;
    }

    const auto closed = closeRegisteredNetworkService(m_registry, descriptor->serviceName);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!closed) {
        Q_EMIT statusTextChanged(QString("Network service is not registered: %1").arg(displayName));
        return;
    }

    Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), false);
    Q_EMIT networkEndpointsChanged();
    Q_EMIT statusTextChanged(QString("Network service closed: %1").arg(displayName));
}

}  // namespace Dss::Ui
