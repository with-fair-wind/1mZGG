#include "dss/ui/serial_port_view_model.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <memory>
#include <string_view>

#include "dss/comm/i_serial_channel.h"
#include "dss/comm/serial_command_interfaces.h"
#include "dss/core/config.h"
#include "dss/core/types.h"

namespace Dss::Ui {
namespace {

/// 串口波特率最大值，覆盖常见 USB/高速串口配置。
constexpr int kMaxSerialBaudRate = 4'000'000;

/// 串口协议单字节字段最大值。
constexpr int kMaxSerialByte = 0xFF;

/// 曝光延迟字段最大值（24 bit）。
constexpr int kMaxExposureDelayTicks = 0xFF'FFFF;

/// @brief CommNetConfig 中串口配置成员指针类型。
using SerialMember = Dss::Core::SerialConfig Dss::Core::CommNetConfig::*;

/// @brief 可由 UI 显式打开/关闭的串口通道描述。
struct SerialChannelDescriptor {
    std::string_view key;          ///< 串口通道键名。
    std::string_view serviceName;  ///< 服务注册名。
    std::string_view displayName;  ///< 界面显示名称。
    SerialMember member;           ///< 对应配置成员。
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

/// @brief 获取已注册的统一串口通道服务。
[[nodiscard]] auto registeredSerialChannel(Dss::Core::ServiceRegistry& registry,
                                           std::string_view name)
    -> std::shared_ptr<Dss::Comm::ISerialChannel> {
    return registry.tryGet<Dss::Comm::ISerialChannel>(name);
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

/// @brief 判断串口波特率是否处于可配置范围。
[[nodiscard]] auto isSerialBaudRateInRange(int baudRate) -> bool {
    return baudRate > 0 && baudRate <= kMaxSerialBaudRate;
}

/// @brief 判断串口数据位是否处于常用 Qt 串口范围。
[[nodiscard]] auto isSerialDataBitsInRange(int dataBits) -> bool {
    return dataBits >= 5 && dataBits <= 8;
}

/// @brief 判断串口停止位是否处于当前配置支持范围。
[[nodiscard]] auto isSerialStopBitsInRange(int stopBits) -> bool {
    return stopBits == 1 || stopBits == 2;
}

/// @brief 判断串口协议单字节字段是否处于 0-255 范围。
[[nodiscard]] auto isSerialByteInRange(int value) -> bool {
    return value >= 0 && value <= kMaxSerialByte;
}

/// @brief 判断曝光延迟字段是否处于 24 bit 范围。
[[nodiscard]] auto isExposureDelayTicksInRange(int ticks) -> bool {
    return ticks >= 0 && ticks <= kMaxExposureDelayTicks;
}

/// @brief 判断时间戳日期字段是否处于主控状态回包支持范围。
[[nodiscard]] auto isTimestampDateInRange(int year, int month, int day) -> bool {
    return year >= 2000 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

/// @brief 判断时间戳时间字段是否处于主控状态回包支持范围。
[[nodiscard]] auto isTimestampTimeInRange(int hour, int minute, int second, int millisecond)
    -> bool {
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59 &&
           millisecond >= 0 && millisecond <= 999;
}

/// @brief 判断角度是否处于 AE 编码支持范围。
[[nodiscard]] auto isAngleDegreesInRange(double degrees) -> bool {
    return std::isfinite(degrees) && degrees >= 0.0 && degrees <= 360.0;
}

/// @brief 判断浮点修正参数是否均为有限值。
[[nodiscard]] auto areFiniteCorrections(double first, double second, double third, double fourth)
    -> bool {
    return std::isfinite(first) && std::isfinite(second) && std::isfinite(third) &&
           std::isfinite(fourth);
}

/// @brief 根据 UI 输入构造串口配置。
[[nodiscard]] auto makeSerialConfig(const QString& portName, int baudRate, int dataBits,
                                    int stopBits) -> Dss::Core::SerialConfig {
    return Dss::Core::SerialConfig{
        .portName = portName.trimmed().toStdString(),
        .baudRate = baudRate,
        .dataBits = dataBits,
        .stopBits = stopBits,
    };
}

/// @brief 检查串口命令服务是否具备发送条件。
[[nodiscard]] auto serialCommandUnavailableMessage(Dss::Core::ServiceRegistry& registry,
                                                   std::string_view serviceName,
                                                   std::string_view displayName) -> QString {
    const auto service = registeredSerialChannel(registry, serviceName);
    const auto display = descriptorText(displayName);
    if (!service) {
        return QString("Serial channel is not registered: %1").arg(display);
    }
    if (!service->isOpen()) {
        return QString("Serial channel is not open: %1").arg(display);
    }
    return {};
}

/// @brief 根据 UI 参数构造伺服修正 DTO。
[[nodiscard]] auto makeServoCorrection(bool distanceValid, bool speedValid, double distanceXArcsec,
                                       double distanceYArcsec, double speedXArcsecPerSec,
                                       double speedYArcsecPerSec, int mode)
    -> Dss::Comm::ServoCorrection {
    return Dss::Comm::ServoCorrection{
        .distanceValid = distanceValid,
        .speedValid = speedValid,
        .distanceArcsec = Dss::Core::Vec2f{static_cast<float>(distanceXArcsec),
                                           static_cast<float>(distanceYArcsec)},
        .speedArcsecPerSec = Dss::Core::Vec2f{static_cast<float>(speedXArcsecPerSec),
                                              static_cast<float>(speedYArcsecPerSec)},
        .mode = static_cast<std::uint8_t>(mode),
    };
}

}  // namespace

SerialPortViewModel::SerialPortViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

SerialPortViewModel::~SerialPortViewModel() = default;

auto SerialPortViewModel::serialChannelConfigs() -> std::vector<SerialChannelState> {
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

bool SerialPortViewModel::isSerialChannelOpen(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        return false;
    }

    const auto service = registeredSerialChannel(m_registry, descriptor->serviceName);
    return service && service->isOpen();
}

bool SerialPortViewModel::openSerialChannel(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown serial channel: %1").arg(key));
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();
    const auto& serialConfig = commNet.*(descriptor->member);
    auto result = openRegisteredSerialChannel(m_registry, descriptor->serviceName, serialConfig);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!result.has_value()) {
        if (result.error() == "service is not registered") {
            Q_EMIT statusTextChanged(QString("Serial channel is not registered: %1").arg(displayName));
        } else {
            Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        }
        Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), false);
        Q_EMIT serialChannelsChanged();
        return false;
    }

    Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), true);
    Q_EMIT serialChannelsChanged();
    Q_EMIT statusTextChanged(QString("Serial channel opened: %1").arg(displayName));
    return true;
}

void SerialPortViewModel::closeSerialChannel(const QString& key) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown serial channel: %1").arg(key));
        return;
    }

    const auto closed = closeRegisteredSerialChannel(m_registry, descriptor->serviceName);

    const auto displayName = descriptorText(descriptor->displayName);
    if (!closed) {
        Q_EMIT statusTextChanged(QString("Serial channel is not registered: %1").arg(displayName));
        return;
    }

    Q_EMIT serialChannelStateChanged(descriptorText(descriptor->key), false);
    Q_EMIT serialChannelsChanged();
    Q_EMIT statusTextChanged(QString("Serial channel closed: %1").arg(displayName));
}

bool SerialPortViewModel::applySerialChannelConfig(const QString& key, const QString& portName,
                                                   int baudRate, int dataBits, int stopBits) {
    const auto* descriptor = findSerialChannelDescriptor(key);
    if (descriptor == nullptr) {
        Q_EMIT statusTextChanged(QString("Unknown serial channel: %1").arg(key));
        return false;
    }
    if (portName.trimmed().isEmpty()) {
        Q_EMIT statusTextChanged("Serial port name must not be empty");
        return false;
    }
    if (!isSerialBaudRateInRange(baudRate)) {
        Q_EMIT statusTextChanged(QString("Serial baud rate must be 1-%1").arg(kMaxSerialBaudRate));
        return false;
    }
    if (!isSerialDataBitsInRange(dataBits)) {
        Q_EMIT statusTextChanged("Serial data bits must be 5-8");
        return false;
    }
    if (!isSerialStopBitsInRange(stopBits)) {
        Q_EMIT statusTextChanged("Serial stop bits must be 1 or 2");
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
    Q_EMIT statusTextChanged(
        QString("Serial channel config applied: %1").arg(descriptorText(descriptor->displayName)));
    return true;
}

bool SerialPortViewModel::sendExposureCommand(bool freeRun, int frameFrequencyCode,
                                              int exposureDelayTicks) {
    const auto unavailable = serialCommandUnavailableMessage(m_registry, "exposure", "Exposure");
    if (!unavailable.isEmpty()) {
        Q_EMIT statusTextChanged(unavailable);
        return false;
    }
    if (!isSerialByteInRange(frameFrequencyCode)) {
        Q_EMIT statusTextChanged("Exposure frame frequency code must be 0-255");
        return false;
    }
    if (!isExposureDelayTicksInRange(exposureDelayTicks)) {
        Q_EMIT statusTextChanged("Exposure delay ticks must be 0-16777215");
        return false;
    }

    auto commandPort = m_registry.tryGet<Dss::Comm::IExposureCommandPort>("exposure");
    if (!commandPort) {
        Q_EMIT statusTextChanged("Exposure command service is not registered");
        return false;
    }

    commandPort->sendExposureCommand(Dss::Comm::ExposureCommand{
        .triggerMode = freeRun ? Dss::Comm::ExposureTriggerMode::FreeRun
                               : Dss::Comm::ExposureTriggerMode::External,
        .frameFrequencyCode = static_cast<std::uint8_t>(frameFrequencyCode),
        .exposureDelayTicks = static_cast<std::uint32_t>(exposureDelayTicks),
    });
    Q_EMIT statusTextChanged("Exposure command sent");
    return true;
}

bool SerialPortViewModel::sendServoCorrection(bool distanceValid, bool speedValid,
                                              double distanceXArcsec, double distanceYArcsec,
                                              double speedXArcsecPerSec,
                                              double speedYArcsecPerSec, int mode) {
    const auto unavailable = serialCommandUnavailableMessage(m_registry, "servo", "Servo");
    if (!unavailable.isEmpty()) {
        Q_EMIT statusTextChanged(unavailable);
        return false;
    }
    if (!isSerialByteInRange(mode)) {
        Q_EMIT statusTextChanged("Servo mode must be 0-255");
        return false;
    }
    if (!areFiniteCorrections(distanceXArcsec, distanceYArcsec, speedXArcsecPerSec,
                              speedYArcsecPerSec)) {
        Q_EMIT statusTextChanged("Servo correction values must be finite");
        return false;
    }

    auto correctionPort = m_registry.tryGet<Dss::Comm::IServoCorrectionPort>("servo");
    if (!correctionPort) {
        Q_EMIT statusTextChanged("Servo correction service is not registered");
        return false;
    }

    correctionPort->sendServoCorrection(
        makeServoCorrection(distanceValid, speedValid, distanceXArcsec, distanceYArcsec,
                            speedXArcsecPerSec, speedYArcsecPerSec, mode));
    Q_EMIT statusTextChanged("Servo correction sent");
    return true;
}

bool SerialPortViewModel::sendMasterControlStatus(
    int year, int month, int day, int hour, int minute, int second, int millisecond,
    double azimuthDegrees, double elevationDegrees, bool distanceValid, bool speedValid,
    double distanceXArcsec, double distanceYArcsec, double speedXArcsecPerSec,
    double speedYArcsecPerSec, int servoMode) {
    const auto unavailable =
        serialCommandUnavailableMessage(m_registry, "master_control", "Master Control");
    if (!unavailable.isEmpty()) {
        Q_EMIT statusTextChanged(unavailable);
        return false;
    }
    if (!isTimestampDateInRange(year, month, day)) {
        Q_EMIT statusTextChanged("Master control timestamp date is out of range");
        return false;
    }
    if (!isTimestampTimeInRange(hour, minute, second, millisecond)) {
        Q_EMIT statusTextChanged("Master control timestamp time is out of range");
        return false;
    }
    if (!isAngleDegreesInRange(azimuthDegrees) || !isAngleDegreesInRange(elevationDegrees)) {
        Q_EMIT statusTextChanged("Master control pointing angles must be 0-360");
        return false;
    }
    if (!isSerialByteInRange(servoMode)) {
        Q_EMIT statusTextChanged("Master control servo mode must be 0-255");
        return false;
    }
    if (!areFiniteCorrections(distanceXArcsec, distanceYArcsec, speedXArcsecPerSec,
                              speedYArcsecPerSec)) {
        Q_EMIT statusTextChanged("Master control correction values must be finite");
        return false;
    }

    auto statusPort = m_registry.tryGet<Dss::Comm::IMasterControlStatusPort>("master_control");
    if (!statusPort) {
        Q_EMIT statusTextChanged("Master control status service is not registered");
        return false;
    }

    statusPort->sendMasterControlStatus(Dss::Comm::MasterControlStatus{
        .correction =
            makeServoCorrection(distanceValid, speedValid, distanceXArcsec, distanceYArcsec,
                                speedXArcsecPerSec, speedYArcsecPerSec, servoMode),
        .timestamp = Dss::Core::Timestamp{year, month, day, hour, minute, second, millisecond},
        .pointingAe = Dss::Core::Vec2f{static_cast<float>(azimuthDegrees),
                                       static_cast<float>(elevationDegrees)},
    });
    Q_EMIT statusTextChanged("Master control status sent");
    return true;
}

}  // namespace Dss::Ui
