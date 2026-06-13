#include "dss/ui/view_model.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/core/config.h"
#include "dss/network/data_exchange.h"
#include "dss/network/i_network_channel.h"
#include "dss/processing/image_processor.h"
#ifdef DSS_HAS_OPENCV
#include "dss/processing/opencv_processing_strategy.h"
#endif
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"
#include "dss/tracking/manual_tracker.h"
#include "dss/tracking/track_manager.h"

namespace Dss::Ui {
namespace {

/// UDP 端口最大值。
constexpr int kMaxUdpPort = 65535;

/// 串口波特率最大值，覆盖常见 USB/高速串口配置。
constexpr int kMaxSerialBaudRate = 4'000'000;

/// @brief CommNetConfig 中 UDP 端点成员指针类型。
using EndpointMember = Dss::Core::UdpEndpointConfig Dss::Core::CommNetConfig::*;

/// @brief CommNetConfig 中串口配置成员指针类型。
using SerialMember = Dss::Core::SerialConfig Dss::Core::CommNetConfig::*;

/// @brief 可编辑网络端点描述。
struct NetworkEndpointDescriptor {
    std::string_view key;          ///< 端点键名
    std::string_view displayName;  ///< 界面显示名称
    EndpointMember member;         ///< 对应配置成员
};

/// @brief 可由 UI 显式打开/关闭的网络服务描述。
struct NetworkServiceDescriptor {
    std::string_view key;          ///< 端点键名
    std::string_view serviceName;  ///< 服务注册名
    std::string_view displayName;  ///< 界面显示名称
    EndpointMember member;         ///< 对应配置成员
};

/// @brief 可由 UI 显式打开/关闭的串口通道描述。
struct SerialChannelDescriptor {
    std::string_view key;          ///< 串口通道键名
    std::string_view serviceName;  ///< 服务注册名
    std::string_view displayName;  ///< 界面显示名称
    SerialMember member;           ///< 对应配置成员
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

/// 可由 UI 显式打开/关闭的串口通道描述表。
constexpr std::array kSerialChannelDescriptors{
    SerialChannelDescriptor{"display", "display", "Display",
                            &Dss::Core::CommNetConfig::displayPort},
    SerialChannelDescriptor{"exposure", "exposure", "Exposure",
                            &Dss::Core::CommNetConfig::exposurePort},
    SerialChannelDescriptor{"master_control", "master_control", "Master Control",
                            &Dss::Core::CommNetConfig::masterControlPort},
    SerialChannelDescriptor{"servo", "servo", "Servo", &Dss::Core::CommNetConfig::servoPort},
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

/// @brief 查找可控串口通道描述。
[[nodiscard]] auto findSerialChannelDescriptor(const QString& key)
    -> const SerialChannelDescriptor* {
    const auto normalizedKey = key.trimmed().toStdString();
    for (const auto& descriptor : kSerialChannelDescriptors) {
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

/// @brief 获取已注册的统一串口通道服务。
[[nodiscard]] auto registeredSerialChannel(Dss::Core::ServiceRegistry& registry,
                                           std::string_view name)
    -> std::shared_ptr<Dss::Comm::ISerialChannel> {
    return registry.tryGet<Dss::Comm::ISerialChannel>(name);
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

/// @brief 打开已注册的统一串口通道服务。
[[nodiscard]] auto openRegisteredSerialChannel(Dss::Core::ServiceRegistry& registry,
                                               std::string_view name,
                                               const Dss::Core::SerialConfig& config)
    -> std::expected<void, std::string> {
    auto service = registeredSerialChannel(registry, name);
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

/// @brief 关闭已注册的统一串口通道服务。
[[nodiscard]] auto closeRegisteredSerialChannel(Dss::Core::ServiceRegistry& registry,
                                                std::string_view name) -> bool {
    auto service = registeredSerialChannel(registry, name);
    if (!service) {
        return false;
    }
    service->close();
    return true;
}

/// @brief 读取端点本地 IP
/// @return Qt 字符串
[[nodiscard]] auto endpointLocalIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString {
    return QString::fromStdString(endpoint.localIp);
}

/// @brief 读取端点远端 IP
/// @return Qt 字符串
[[nodiscard]] auto endpointRemoteIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString {
    return QString::fromStdString(endpoint.remoteIp);
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

/// @brief 将串口配置和服务状态转换为 UI 状态。
[[nodiscard]] auto makeSerialChannelState(const SerialChannelDescriptor& descriptor,
                                          const Dss::Core::SerialConfig& config,
                                          const std::shared_ptr<Dss::Comm::ISerialChannel>& channel)
    -> SerialChannelState {
    return SerialChannelState{
        .key = descriptorText(descriptor.key),
        .displayName = descriptorText(descriptor.displayName),
        .portName = QString::fromStdString(config.portName),
        .baudRate = config.baudRate,
        .dataBits = config.dataBits,
        .stopBits = config.stopBits,
        .recvFrameSize = channel ? channel->recvFrameSize() : 0U,
        .sendFrameSize = channel ? channel->sendFrameSize() : 0U,
        .isRegistered = channel != nullptr,
        .isOpen = channel && channel->isOpen(),
    };
}

/// @brief 判断本地端口是否处于可配置范围
/// @return 端口合法时返回 true
[[nodiscard]] auto isLocalPortInRange(int port) -> bool {
    return port >= 0 && port <= kMaxUdpPort;
}

/// @brief 判断远端端口是否处于可配置范围
/// @return 端口合法时返回 true
[[nodiscard]] auto isRemotePortInRange(int port) -> bool {
    return port > 0 && port <= kMaxUdpPort;
}

/// @brief 判断串口波特率是否处于可配置范围
/// @return 波特率合法时返回 true
[[nodiscard]] auto isSerialBaudRateInRange(int baudRate) -> bool {
    return baudRate > 0 && baudRate <= kMaxSerialBaudRate;
}

/// @brief 判断串口数据位是否处于常用 Qt 串口范围
/// @return 数据位合法时返回 true
[[nodiscard]] auto isSerialDataBitsInRange(int dataBits) -> bool {
    return dataBits >= 5 && dataBits <= 8;
}

/// @brief 判断串口停止位是否处于当前配置支持范围
/// @return 停止位合法时返回 true
[[nodiscard]] auto isSerialStopBitsInRange(int stopBits) -> bool {
    return stopBits == 1 || stopBits == 2;
}

/// @brief 根据 UI 输入构造 UDP 端点配置
/// @return 规范化后的 UDP 端点配置
[[nodiscard]] auto makeEndpointConfig(const QString& localIp, int localPort,
                                      const QString& remoteIp, int remotePort)
    -> Dss::Core::UdpEndpointConfig {
    Dss::Core::UdpEndpointConfig endpoint{};
    endpoint.localIp = localIp.trimmed().toStdString();
    endpoint.localPort = static_cast<std::uint16_t>(localPort);
    endpoint.remoteIp = remoteIp.trimmed().toStdString();
    endpoint.remotePort = static_cast<std::uint16_t>(remotePort);
    return endpoint;
}

/// @brief 根据 UI 输入构造串口配置
/// @return 规范化后的串口配置
[[nodiscard]] auto makeSerialConfig(const QString& portName, int baudRate, int dataBits,
                                    int stopBits) -> Dss::Core::SerialConfig {
    return Dss::Core::SerialConfig{
        .portName = portName.trimmed().toStdString(),
        .baudRate = baudRate,
        .dataBits = dataBits,
        .stopBits = stopBits,
    };
}

}  // namespace

ViewModel::ViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry, QObject* parent)
    : QObject(parent), m_bus(bus), m_registry(registry) {
    setupSubscriptions();
}

ViewModel::~ViewModel() = default;

auto ViewModel::networkEndpointConfigs() -> std::vector<NetworkEndpointState> {
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

bool ViewModel::isNetworkEndpointOpen(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        return false;
    }

    const auto service = registeredNetworkService(m_registry, descriptor->serviceName);
    return service && service->isOpen();
}

auto ViewModel::serialChannelConfigs() -> std::vector<SerialChannelState> {
    const auto& commNet = Dss::Core::Config::instance().commNet();
    std::vector<SerialChannelState> channels;
    channels.reserve(kSerialChannelDescriptors.size());
    for (const auto& descriptor : kSerialChannelDescriptors) {
        channels.push_back(
            makeSerialChannelState(descriptor, commNet.*(descriptor.member),
                                   registeredSerialChannel(m_registry, descriptor.serviceName)));
    }
    return channels;
}

bool ViewModel::isSerialChannelOpen(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        return false;
    }

    const auto service = registeredSerialChannel(m_registry, descriptor->serviceName);
    return service && service->isOpen();
}

auto ViewModel::dataExchangeGxtcLocalIp() const -> QString {
    return endpointLocalIp(Dss::Core::Config::instance().commNet().exchangeGxtc);
}

int ViewModel::dataExchangeGxtcLocalPort() const {
    return Dss::Core::Config::instance().commNet().exchangeGxtc.localPort;
}

auto ViewModel::dataExchangeGxtcRemoteIp() const -> QString {
    return endpointRemoteIp(Dss::Core::Config::instance().commNet().exchangeGxtc);
}

int ViewModel::dataExchangeGxtcRemotePort() const {
    return Dss::Core::Config::instance().commNet().exchangeGxtc.remotePort;
}

auto ViewModel::dataExchangeGdclLocalIp() const -> QString {
    return endpointLocalIp(Dss::Core::Config::instance().commNet().exchangeGdcl);
}

int ViewModel::dataExchangeGdclLocalPort() const {
    return Dss::Core::Config::instance().commNet().exchangeGdcl.localPort;
}

auto ViewModel::dataExchangeGdclRemoteIp() const -> QString {
    return endpointRemoteIp(Dss::Core::Config::instance().commNet().exchangeGdcl);
}

int ViewModel::dataExchangeGdclRemotePort() const {
    return Dss::Core::Config::instance().commNet().exchangeGdcl.remotePort;
}

/**
 * @brief 加载回放文件列表并初始化序列源
 * @param files 图像文件路径列表
 * @return 成功返回 true
 */
bool ViewModel::selectReplayFiles(const QStringList& files) {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        setStatus("Replay source is not registered");
        return false;
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const auto& file : files) {
        if (!file.isEmpty()) {
            paths.emplace_back(file.toStdWString());
        }
    }

    auto result = replaySource->setFiles(std::move(paths));
    if (!result.has_value()) {
        m_replayFrameCount = 0;
        setReplayCurrentFrame(0);
        Q_EMIT replayFrameCountChanged(m_replayFrameCount);
        setStatus(QString::fromStdString(result.error()));
        return false;
    }

    auto initResult = replaySource->init();
    if (!initResult.has_value()) {
        m_replayFrameCount = 0;
        setReplayCurrentFrame(0);
        Q_EMIT replayFrameCountChanged(m_replayFrameCount);
        setStatus(QString::fromStdString(initResult.error()));
        return false;
    }

    m_replayFrameCount = static_cast<int>(replaySource->frameCount());
    setReplayCurrentFrame(0);
    Q_EMIT replayFrameCountChanged(m_replayFrameCount);
    setStatus(QString("Sequence selected: %1 frames").arg(m_replayFrameCount));
    return true;
}

/// 启动帧源与图像处理器，进入回放状态
void ViewModel::startGrab() {
    if (m_grabbing) {
        return;
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (!processor || !frameSource) {
        setStatus("Replay services are not registered");
        return;
    }

    auto initResult = frameSource->init();
    if (!initResult.has_value()) {
        setStatus(QString::fromStdString(initResult.error()));
        return;
    }

    setReplayCurrentFrame(0);
    processor->start();
    frameSource->start();
    m_grabbing = true;
    m_statusText = "Replaying...";
    Q_EMIT grabbingChanged(true);
    Q_EMIT statusTextChanged(m_statusText);
    m_bus.emit(Dss::Core::GrabStartedEvent{frameSource->frameWidth(), frameSource->frameHeight()});
}

/// 停止帧源与图像处理器，退出回放状态
void ViewModel::stopGrab() {
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (frameSource) {
        frameSource->stop();
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (processor) {
        processor->stop();
    }

    m_grabbing = false;
    m_statusText = "Stopped";
    Q_EMIT grabbingChanged(false);
    Q_EMIT statusTextChanged(m_statusText);
    m_bus.emit(Dss::Core::GrabStoppedEvent{});
}

/// 单步前进一帧；若正在连续回放则先停止
bool ViewModel::stepReplayForward() {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        setStatus("Replay source is not registered");
        return false;
    }

    auto result = replaySource->stepForward();
    if (!result.has_value()) {
        setStatus(QString::fromStdString(result.error()));
        return false;
    }
    return true;
}

void ViewModel::setProcessingMode(int mode) {
    if (m_processingMode != mode) {
        m_processingMode = mode;
        Q_EMIT processingModeChanged(mode);
    }
    configureProcessingStrategy();
}

void ViewModel::setTrackMode(int mode) {
    if (m_trackMode != mode) {
        m_trackMode = mode;
        Q_EMIT trackModeChanged(mode);
        configureTrackingStrategy();
    }
}

void ViewModel::setExposure(double ms) {
    if (m_exposure != ms) {
        m_exposure = ms;
        Q_EMIT exposureChanged(ms);
    }
}

/// 记录手动目标并切换/刷新手动跟踪策略
void ViewModel::selectTarget(QPointF pos) {
    m_manualTarget = makeManualTarget(pos);
    m_bus.emit(Dss::Core::ManualTargetSelectEvent{static_cast<float>(pos.x()),
                                                  static_cast<float>(pos.y())});
    if (m_trackMode != static_cast<int>(Dss::Core::TrackMode::Manual)) {
        setTrackMode(static_cast<int>(Dss::Core::TrackMode::Manual));
    } else {
        configureTrackingStrategy();
    }
    setStatus(
        QString("Manual target selected: %1, %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
}

/**
 * @brief 启动图像与跟踪数据本地存储
 * 若存储后端未就绪则先初始化；任一环节失败则回滚已启动的后端
 */
void ViewModel::startSaving() {
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
    if (!storage) {
        setStatus("Image storage is not registered");
        return;
    }
    if (!storage->isReady()) {
        auto initResult = storage->init(storage->baseDir());
        if (!initResult.has_value()) {
            setStatus(QString::fromStdString(initResult.error()));
            return;
        }
    }
    auto startResult = storage->start();
    if (!startResult.has_value()) {
        setStatus(QString::fromStdString(startResult.error()));
        return;
    }

    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage) {
        if (!trackStorage->isReady()) {
            auto initTrackResult = trackStorage->init(trackStorage->baseDir());
            if (!initTrackResult.has_value()) {
                storage->stop();
                setStatus(QString::fromStdString(initTrackResult.error()));
                return;
            }
        }
        auto startTrackResult = trackStorage->start();
        if (!startTrackResult.has_value()) {
            storage->stop();
            setStatus(QString::fromStdString(startTrackResult.error()));
            return;
        }
    }

    m_saving = true;
    Q_EMIT savingChanged(true);
    setStatus("Saving enabled");
}

/// 停止图像与跟踪数据存储
void ViewModel::stopSaving() {
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
    if (storage) {
        storage->stop();
    }
    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage) {
        trackStorage->stop();
    }
    m_saving = false;
    Q_EMIT savingChanged(false);
    setStatus("Saving stopped");
}

/// 校验并应用单个 UDP 网络端点配置
bool ViewModel::applyNetworkEndpointConfig(const QString& key, const QString& localIp,
                                           int localPort, const QString& remoteIp, int remotePort) {
    const auto* descriptor = findNetworkEndpointDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown network endpoint: %1").arg(key));
        return false;
    }
    if (!isLocalPortInRange(localPort)) {
        setStatus("Network endpoint local port must be 0-65535");
        return false;
    }
    if (!isRemotePortInRange(remotePort)) {
        setStatus("Network endpoint remote port must be 1-65535");
        return false;
    }

    auto& commNet = Dss::Core::Config::instance().mutableCommNet();
    commNet.*(descriptor->member) = makeEndpointConfig(localIp, localPort, remoteIp, remotePort);
    if (descriptor->member == &Dss::Core::CommNetConfig::exchangeGxtc) {
        commNet.exchange = commNet.exchangeGxtc;
    }

    if (descriptor->member == &Dss::Core::CommNetConfig::exchangeGxtc ||
        descriptor->member == &Dss::Core::CommNetConfig::exchangeGdcl) {
        if (auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
            dataExchange && dataExchange->isOpen()) {
            dataExchange->close();
            setDataExchangeOpen(false);
        }
        Q_EMIT dataExchangeEndpointsChanged();
    }
    if (findNetworkServiceDescriptor(key) != nullptr && isNetworkEndpointOpen(key)) {
        closeNetworkEndpoint(key);
    }

    Q_EMIT networkEndpointsChanged();
    setStatus(QString("Network endpoint applied: %1")
                  .arg(QString::fromUtf8(descriptor->displayName.data(),
                                         static_cast<qsizetype>(descriptor->displayName.size()))));
    return true;
}

/// 显式打开指定端点对应的网络服务
bool ViewModel::openNetworkEndpoint(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown network service: %1").arg(key));
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();
    const auto& endpoint = commNet.*(descriptor->member);
    auto result = openRegisteredNetworkService(m_registry, descriptor->serviceName, endpoint);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!result.has_value()) {
        if (result.error() == "service is not registered") {
            setStatus(QString("Network service is not registered: %1").arg(displayName));
        } else {
            setStatus(QString::fromStdString(result.error()));
        }
        Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), false);
        Q_EMIT networkEndpointsChanged();
        return false;
    }

    Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), true);
    Q_EMIT networkEndpointsChanged();
    setStatus(QString("Network service opened: %1").arg(displayName));
    return true;
}

/// 显式关闭指定端点对应的网络服务
void ViewModel::closeNetworkEndpoint(const QString& key) {
    const auto* descriptor = findNetworkServiceDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown network service: %1").arg(key));
        return;
    }

    const auto closed = closeRegisteredNetworkService(m_registry, descriptor->serviceName);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!closed) {
        setStatus(QString("Network service is not registered: %1").arg(displayName));
        return;
    }

    Q_EMIT networkServiceStateChanged(descriptorText(descriptor->key), false);
    Q_EMIT networkEndpointsChanged();
    setStatus(QString("Network service closed: %1").arg(displayName));
}

/// 显式打开指定串口通道服务
bool ViewModel::openSerialChannel(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown serial channel: %1").arg(key));
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();
    const auto& serialConfig = commNet.*(descriptor->member);
    auto result = openRegisteredSerialChannel(m_registry, descriptor->serviceName, serialConfig);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!result.has_value()) {
        if (result.error() == "service is not registered") {
            setStatus(QString("Serial channel is not registered: %1").arg(displayName));
        } else {
            setStatus(QString::fromStdString(result.error()));
        }
        Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), false);
        Q_EMIT serialChannelsChanged();
        return false;
    }

    Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), true);
    Q_EMIT serialChannelsChanged();
    setStatus(QString("Serial channel opened: %1").arg(displayName));
    return true;
}

/// 显式关闭指定串口通道服务
void ViewModel::closeSerialChannel(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown serial channel: %1").arg(key));
        return;
    }

    const auto closed = closeRegisteredSerialChannel(m_registry, descriptor->serviceName);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!closed) {
        setStatus(QString("Serial channel is not registered: %1").arg(displayName));
        return;
    }

    Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), false);
    Q_EMIT serialChannelsChanged();
    setStatus(QString("Serial channel closed: %1").arg(displayName));
}

/// 校验并应用单个串口通道配置
bool ViewModel::applySerialChannelConfig(const QString& key, const QString& portName, int baudRate,
                                         int dataBits, int stopBits) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        setStatus(QString("Unknown serial channel: %1").arg(key));
        return false;
    }
    if (portName.trimmed().isEmpty()) {
        setStatus("Serial port name must not be empty");
        return false;
    }
    if (!isSerialBaudRateInRange(baudRate)) {
        setStatus(QString("Serial baud rate must be 1-%1").arg(kMaxSerialBaudRate));
        return false;
    }
    if (!isSerialDataBitsInRange(dataBits)) {
        setStatus("Serial data bits must be 5-8");
        return false;
    }
    if (!isSerialStopBitsInRange(stopBits)) {
        setStatus("Serial stop bits must be 1 or 2");
        return false;
    }

    auto& commNet = Dss::Core::Config::instance().mutableCommNet();
    commNet.*(descriptor->member) = makeSerialConfig(portName, baudRate, dataBits, stopBits);

    auto service = registeredSerialChannel(m_registry, descriptor->serviceName);
    if (service && service->isOpen()) {
        service->close();
        Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), false);
    }

    Q_EMIT serialChannelsChanged();
    setStatus(
        QString("Serial channel config applied: %1").arg(descriptorText(descriptor->displayName)));
    return true;
}

/// 校验并应用 GXTC/GDCL 数据交换端点配置
bool ViewModel::applyDataExchangeEndpoints(const QString& gxtcLocalIp, int gxtcLocalPort,
                                           const QString& gxtcRemoteIp, int gxtcRemotePort,
                                           const QString& gdclLocalIp, int gdclLocalPort,
                                           const QString& gdclRemoteIp, int gdclRemotePort) {
    if (!isLocalPortInRange(gxtcLocalPort) || !isLocalPortInRange(gdclLocalPort)) {
        setStatus("Data exchange local ports must be 0-65535");
        return false;
    }
    if (!isRemotePortInRange(gxtcRemotePort) || !isRemotePortInRange(gdclRemotePort)) {
        setStatus("Data exchange remote ports must be 1-65535");
        return false;
    }

    auto& commNet = Dss::Core::Config::instance().mutableCommNet();
    commNet.exchangeGxtc =
        makeEndpointConfig(gxtcLocalIp, gxtcLocalPort, gxtcRemoteIp, gxtcRemotePort);
    commNet.exchangeGdcl =
        makeEndpointConfig(gdclLocalIp, gdclLocalPort, gdclRemoteIp, gdclRemotePort);
    commNet.exchange = commNet.exchangeGxtc;

    if (auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
        dataExchange && dataExchange->isOpen()) {
        dataExchange->close();
        setDataExchangeOpen(false);
    }

    Q_EMIT dataExchangeEndpointsChanged();
    Q_EMIT networkEndpointsChanged();
    setStatus("Data exchange endpoints applied");
    return true;
}

/// 显式打开数据交换服务
bool ViewModel::openDataExchange() {
    auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
    if (!dataExchange) {
        setDataExchangeOpen(false);
        setStatus("Data exchange service is not registered");
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();

    auto result = dataExchange->open(commNet.exchangeGxtc, commNet.exchangeGdcl);
    if (!result.has_value()) {
        dataExchange->close();
        setDataExchangeOpen(false);
        setStatus(QString::fromStdString(result.error()));
        return false;
    }

    setDataExchangeOpen(dataExchange->isOpen());
    if (!m_dataExchangeOpen) {
        dataExchange->close();
        setStatus("Data exchange UDP open failed");
        return false;
    }

    setStatus("Data exchange UDP opened");
    return true;
}

/// 显式关闭数据交换服务
void ViewModel::closeDataExchange() {
    auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
    if (dataExchange) {
        dataExchange->close();
    }
    setDataExchangeOpen(false);
    setStatus("Data exchange UDP closed");
}

void ViewModel::toggleZoom(int level) {
    m_bus.emit(Dss::Core::ZoomChangeEvent{level});
}

/// 订阅显示刷新、处理完成、跟踪结果、主控和网络错误事件
void ViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [this](const Dss::Core::ProcessingCompleteEvent& e) { onProcessingComplete(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [this](const Dss::Core::TrackResultEvent& e) { onTrackResult(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::MasterControlEvent>(
        [this](const Dss::Core::MasterControlEvent& e) { onMasterControl(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>(
        [this](const Dss::Core::NetworkTransmissionErrorEvent& e) {
            onNetworkTransmissionError(e);
        }));
}

/**
 * @brief 将 DisplayRefreshEvent 中的灰度缓冲转为 QImage 并通知 UI
 * @param event 显示刷新事件
 */
void ViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    if (!event.displayImage || event.width == 0 || event.height == 0 || event.stride == 0) {
        return;
    }

    const auto expectedSize = static_cast<size_t>(event.stride) * static_cast<size_t>(event.height);
    if (event.displayImage->size() < expectedSize) {
        return;
    }

    QImage image(event.displayImage->data(), static_cast<int>(event.width),
                 static_cast<int>(event.height), static_cast<qsizetype>(event.stride),
                 QImage::Format_Grayscale8);
    setReplayCurrentFrame(static_cast<int>(event.frameSeq) + 1);
    Q_EMIT displayImageReady(image.copy());
}

/// 转发图像统计量至 UI
void ViewModel::onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event) {
    const auto& s = event.stats;
    Q_EMIT imageStatsUpdated(s.minVal, s.maxVal, s.avg, s.stdDev);
}

/// 更新目标数量与首个目标的跟踪信息
void ViewModel::onTrackResult(const Dss::Core::TrackResultEvent& event) {
    Q_EMIT targetListUpdated(static_cast<int>(event.targets.size()));

    if (!event.targets.empty()) {
        const auto& t = event.targets.front();
        auto info = QString("Target: %1 | AZ: %2 EL: %3")
                        .arg(QString::fromStdString(t.targetId))
                        .arg(static_cast<double>(t.predictedPosAe.x), 0, 'f', 4)
                        .arg(static_cast<double>(t.predictedPosAe.y), 0, 'f', 4);
        Q_EMIT trackInfoUpdated(info);
    }
}

/**
 * @brief 响应外部主控指令，同步曝光、跟踪模式、保存与采集状态
 * @param event 主控事件
 */
void ViewModel::onMasterControl(const Dss::Core::MasterControlEvent& event) {
    setExposure(event.exposure);
    setTrackMode(event.trackMode);

    if (event.save && !m_saving) {
        startSaving();
    } else if (!event.save && m_saving) {
        stopSaving();
    }

    if (event.grab && !m_grabbing) {
        startGrab();
    } else if (!event.grab && m_grabbing) {
        stopGrab();
    }
}

/// 将网络发送失败事件同步到状态栏
void ViewModel::onNetworkTransmissionError(const Dss::Core::NetworkTransmissionErrorEvent& event) {
    setStatus(QString("Network send failed (%1): %2")
                  .arg(QString::fromStdString(event.channel))
                  .arg(QString::fromStdString(event.message)));
}

/// 按 ProcessingMode 为 ImageProcessor 绑定或清除处理策略
void ViewModel::configureProcessingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        setStatus("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::ProcessingMode>(m_processingMode);
    switch (mode) {
        case Dss::Core::ProcessingMode::None:
            processor->setProcessingStrategy(nullptr);
            setStatus("Processing disabled");
            break;
        case Dss::Core::ProcessingMode::Direct:
#ifdef DSS_HAS_OPENCV
            processor->setProcessingStrategy(
                std::make_unique<Dss::Processing::OpenCvProcessingStrategy>());
            setStatus("OpenCV processing enabled");
#else
            processor->setProcessingStrategy(nullptr);
            setStatus("OpenCV processing is not available");
#endif
            break;
        case Dss::Core::ProcessingMode::Diff:
            processor->setProcessingStrategy(nullptr);
            setStatus("Processing mode is not available");
            break;
    }
}

/// 按 TrackMode 创建跟踪策略；手动模式时注入已选目标
void ViewModel::configureTrackingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        setStatus("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::TrackMode>(m_trackMode);
    auto strategy =
        Dss::Tracking::makeTrackingStrategy(mode, Dss::Core::Config::instance().trackingSettings());
    if (!strategy) {
        processor->setTrackingStrategy(nullptr);
        setStatus("Tracking disabled");
        return;
    }

    if (auto* manualTracker = dynamic_cast<Dss::Tracking::ManualTracker*>(strategy.get())) {
        if (m_manualTarget.has_value()) {
            manualTracker->setManualTarget(*m_manualTarget);
        }
        processor->setTrackingStrategy(std::move(strategy));
        setStatus(m_manualTarget.has_value() ? "Manual tracking target armed"
                                             : "Manual tracking enabled");
        return;
    }

    processor->setTrackingStrategy(std::move(strategy));
    setStatus("Tracking mode enabled");
}

/// 以点击位置为中心构造固定尺寸的 MeasuredBlob
auto ViewModel::makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.id = "manual";
    blob.centroid = Dss::Core::Vec2f{static_cast<float>(pos.x()), static_cast<float>(pos.y())};
    blob.minX = blob.centroid.x - 10.0F;
    blob.maxX = blob.centroid.x + 10.0F;
    blob.minY = blob.centroid.y - 10.0F;
    blob.maxY = blob.centroid.y + 10.0F;
    blob.area = 100.0F;
    blob.dn = 10000.0F;
    return blob;
}

void ViewModel::setReplayCurrentFrame(int frame) {
    if (m_replayCurrentFrame == frame) {
        return;
    }
    m_replayCurrentFrame = frame;
    Q_EMIT replayCurrentFrameChanged(frame);
}

void ViewModel::setDataExchangeOpen(bool value) {
    if (m_dataExchangeOpen == value) {
        return;
    }
    m_dataExchangeOpen = value;
    Q_EMIT dataExchangeOpenChanged(value);
}

void ViewModel::setStatus(QString text) {
    if (m_statusText == text) {
        return;
    }
    m_statusText = std::move(text);
    Q_EMIT statusTextChanged(m_statusText);
}

}  // namespace Dss::Ui
