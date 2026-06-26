#include <QCoreApplication>
#include <QString>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "dss/comm/i_serial_channel.h"
#include "dss/comm/serial_command_interfaces.h"
#include "dss/core/config.h"
#include "dss/ui/serial_port_view_model.h"

namespace {

/// @brief 创建或复用测试进程中的 QCoreApplication 实例。
auto ensureQCoreApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_serial_port_view_model";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

/// @brief 用于串口 ViewModel 测试的串口通道桩，避免触碰真实串口硬件。
class RecordingSerialChannel : public Dss::Comm::ISerialChannel {
public:
    /// @brief 构造可记录帧长的测试串口通道。
    RecordingSerialChannel(std::size_t recvFrameSize, std::size_t sendFrameSize)
        : recvFrameSize_(recvFrameSize), sendFrameSize_(sendFrameSize) {}

    /// @brief 记录打开参数并切换为已打开状态。
    auto open(const Dss::Comm::SerialConfig& config) -> std::expected<void, std::string> override {
        lastConfig = config;
        openCount += 1;
        isOpen_ = true;
        status_ = Dss::Core::Status::Ok;
        return {};
    }

    /// @brief 记录关闭次数并切换为初始状态。
    void close() override {
        closeCount += 1;
        isOpen_ = false;
        status_ = Dss::Core::Status::Init;
    }

    /// @brief 查询测试串口是否处于打开状态。
    [[nodiscard]] bool isOpen() const override {
        return isOpen_;
    }

    /// @brief 获取测试串口运行状态。
    [[nodiscard]] auto status() const -> Dss::Core::Status override {
        return status_;
    }

    /// @brief 返回测试串口接收帧长。
    [[nodiscard]] auto recvFrameSize() const -> std::size_t override {
        return recvFrameSize_;
    }

    /// @brief 返回测试串口发送帧长。
    [[nodiscard]] auto sendFrameSize() const -> std::size_t override {
        return sendFrameSize_;
    }

    Dss::Comm::SerialConfig lastConfig{};  ///< 最近一次打开串口时收到的配置。
    int openCount = 0;                     ///< 打开调用次数。
    int closeCount = 0;                    ///< 关闭调用次数。

private:
    std::size_t recvFrameSize_ = 0;                       ///< 固定接收帧长。
    std::size_t sendFrameSize_ = 0;                       ///< 固定发送帧长。
    bool isOpen_ = false;                                 ///< 当前打开状态。
    Dss::Core::Status status_ = Dss::Core::Status::Init;  ///< 当前运行状态。
};

/// @brief 可记录曝光命令的串口通道桩。
class RecordingExposureCommandChannel final : public RecordingSerialChannel,
                                              public Dss::Comm::IExposureCommandPort {
public:
    using RecordingSerialChannel::RecordingSerialChannel;

    /// @brief 记录最近一次曝光命令。
    void sendExposureCommand(const Dss::Comm::ExposureCommand& command) override {
        lastExposureCommand = command;
        exposureCommandCount += 1;
    }

    Dss::Comm::ExposureCommand lastExposureCommand{};  ///< 最近一次曝光命令。
    int exposureCommandCount = 0;                      ///< 曝光命令调用次数。
};

/// @brief 可记录伺服修正命令的串口通道桩。
class RecordingServoCommandChannel final : public RecordingSerialChannel,
                                           public Dss::Comm::IServoCorrectionPort {
public:
    using RecordingSerialChannel::RecordingSerialChannel;

    /// @brief 记录最近一次伺服修正命令。
    void sendServoCorrection(const Dss::Comm::ServoCorrection& correction) override {
        lastServoCorrection = correction;
        servoCorrectionCount += 1;
    }

    Dss::Comm::ServoCorrection lastServoCorrection{};  ///< 最近一次伺服修正命令。
    int servoCorrectionCount = 0;                      ///< 伺服修正命令调用次数。
};

/// @brief 可记录主控状态回包的串口通道桩。
class RecordingMasterControlStatusChannel final : public RecordingSerialChannel,
                                                  public Dss::Comm::IMasterControlStatusPort {
public:
    using RecordingSerialChannel::RecordingSerialChannel;

    /// @brief 记录最近一次主控状态回包。
    void sendMasterControlStatus(const Dss::Comm::MasterControlStatus& status) override {
        lastMasterControlStatus = status;
        masterControlStatusCount += 1;
    }

    Dss::Comm::MasterControlStatus lastMasterControlStatus{};  ///< 最近一次主控状态。
    int masterControlStatusCount = 0;                          ///< 主控状态调用次数。
};

/// @brief 按统一串口通道接口注册测试服务。
void registerSerialChannel(Dss::Core::ServiceRegistry& registry, std::string_view name,
                           const std::shared_ptr<Dss::Comm::ISerialChannel>& service) {
    registry.registerService<Dss::Comm::ISerialChannel>(name, service);
}

}  // namespace

TEST(SerialPortViewModel, ListsSerialChannelsFromConfigWithoutOpeningHardware) {
    auto& app = ensureQCoreApplication();
    (void)app;

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM1", 115200, 7, 2};
    config.exposurePort = {"COM2", 230400, 8, 1};
    config.masterControlPort = {"COM3", 460800, 8, 1};
    config.servoPort = {"COM4", 9600, 8, 1};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    registerSerialChannel(registry, "display", std::make_shared<RecordingSerialChannel>(20, 9));
    registerSerialChannel(registry, "exposure", std::make_shared<RecordingSerialChannel>(23, 8));
    registerSerialChannel(registry, "master_control",
                          std::make_shared<RecordingSerialChannel>(30, 28));
    registerSerialChannel(registry, "servo", std::make_shared<RecordingSerialChannel>(20, 14));

    Dss::Ui::SerialPortViewModel serial(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    const auto channels = serial.serialChannelConfigs();

    ASSERT_EQ(channels.size(), 4U);
    EXPECT_EQ(channels[0].key, QString("display"));
    EXPECT_EQ(channels[0].portName, QString("COM1"));
    EXPECT_EQ(channels[0].recvFrameSize, 20U);
    EXPECT_EQ(channels[2].key, QString("master_control"));
    EXPECT_EQ(channels[2].sendFrameSize, 28U);
    EXPECT_EQ(channels[3].key, QString("servo"));
    EXPECT_FALSE(channels[3].isOpen);
}

TEST(SerialPortViewModel, OpensClosesAndAppliesSerialConfig) {
    auto& app = ensureQCoreApplication();
    (void)app;

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM31", 115200, 8, 1};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto display = std::make_shared<RecordingSerialChannel>(20, 9);
    registerSerialChannel(registry, "display", display);

    Dss::Ui::SerialPortViewModel serial(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    ASSERT_TRUE(serial.openSerialChannel("display"));
    EXPECT_TRUE(display->isOpen());
    EXPECT_TRUE(serial.isSerialChannelOpen("display"));
    EXPECT_EQ(display->lastConfig.portName, "COM31");

    ASSERT_TRUE(serial.applySerialChannelConfig("display", " COM32 ", 57600, 7, 2));
    EXPECT_EQ(config.displayPort.portName, "COM32");
    EXPECT_EQ(config.displayPort.baudRate, 57600);
    EXPECT_FALSE(display->isOpen());
    EXPECT_EQ(display->closeCount, 1);

    serial.closeSerialChannel("display");
    EXPECT_FALSE(display->isOpen());
}

TEST(SerialPortViewModel, RejectsInvalidSerialConfig) {
    auto& app = ensureQCoreApplication();
    (void)app;

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM51", 115200, 8, 1};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::SerialPortViewModel serial(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&serial, &Dss::Ui::SerialPortViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_FALSE(serial.applySerialChannelConfig("display", "", 57600, 8, 1));
    EXPECT_EQ(config.displayPort.portName, "COM51");
    EXPECT_EQ(statusText, QString("Serial port name must not be empty"));

    EXPECT_FALSE(serial.applySerialChannelConfig("display", "COM52", 0, 8, 1));
    EXPECT_EQ(config.displayPort.baudRate, 115200);
    EXPECT_EQ(statusText, QString("Serial baud rate must be 1-4000000"));

    EXPECT_FALSE(serial.applySerialChannelConfig("display", "COM52", 57600, 9, 1));
    EXPECT_EQ(config.displayPort.dataBits, 8);
    EXPECT_EQ(statusText, QString("Serial data bits must be 5-8"));
}

TEST(SerialPortViewModel, SendsCommandsThroughOpenCommandServices) {
    auto& app = ensureQCoreApplication();
    (void)app;

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exposurePort = {"COM61", 115200, 8, 1};
    config.servoPort = {"COM62", 115200, 8, 1};
    config.masterControlPort = {"COM63", 115200, 8, 1};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto exposure = std::make_shared<RecordingExposureCommandChannel>(23, 8);
    auto servo = std::make_shared<RecordingServoCommandChannel>(20, 14);
    auto master = std::make_shared<RecordingMasterControlStatusChannel>(30, 28);
    registerSerialChannel(registry, "exposure", exposure);
    registerSerialChannel(registry, "servo", servo);
    registerSerialChannel(registry, "master_control", master);
    registry.registerService<Dss::Comm::IExposureCommandPort>("exposure", exposure);
    registry.registerService<Dss::Comm::IServoCorrectionPort>("servo", servo);
    registry.registerService<Dss::Comm::IMasterControlStatusPort>("master_control", master);

    Dss::Ui::SerialPortViewModel serial(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});
    ASSERT_TRUE(serial.openSerialChannel("exposure"));
    ASSERT_TRUE(serial.openSerialChannel("servo"));
    ASSERT_TRUE(serial.openSerialChannel("master_control"));

    ASSERT_TRUE(serial.sendExposureCommand(true, 7, 0x123456));
    EXPECT_EQ(exposure->exposureCommandCount, 1);
    EXPECT_EQ(exposure->lastExposureCommand.triggerMode, Dss::Comm::ExposureTriggerMode::FreeRun);
    EXPECT_EQ(exposure->lastExposureCommand.frameFrequencyCode, 7);
    EXPECT_EQ(exposure->lastExposureCommand.exposureDelayTicks, 0x123456U);

    ASSERT_TRUE(serial.sendServoCorrection(true, false, 1.5, -2.5, 3.25, -4.5, 0x22));
    EXPECT_EQ(servo->servoCorrectionCount, 1);
    EXPECT_TRUE(servo->lastServoCorrection.distanceValid);
    EXPECT_FLOAT_EQ(servo->lastServoCorrection.distanceArcsec.x, 1.5F);
    EXPECT_FLOAT_EQ(servo->lastServoCorrection.speedArcsecPerSec.y, -4.5F);

    ASSERT_TRUE(serial.sendMasterControlStatus(2026, 6, 13, 9, 10, 11, 120, 12.5, 45.5, true, true,
                                               1.0, -1.5, 2.0, -2.5, 0x19));
    EXPECT_EQ(master->masterControlStatusCount, 1);
    EXPECT_EQ(master->lastMasterControlStatus.timestamp.year, 2026);
    EXPECT_FLOAT_EQ(master->lastMasterControlStatus.pointingAe.x, 12.5F);
    EXPECT_TRUE(master->lastMasterControlStatus.correction.speedValid);
}

TEST(SerialPortViewModel, RejectsCommandWhenChannelIsClosedOrInvalid) {
    auto& app = ensureQCoreApplication();
    (void)app;

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exposurePort = {"COM64", 115200, 8, 1};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto exposure = std::make_shared<RecordingExposureCommandChannel>(23, 8);
    registerSerialChannel(registry, "exposure", exposure);
    registry.registerService<Dss::Comm::IExposureCommandPort>("exposure", exposure);

    Dss::Ui::SerialPortViewModel serial(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&serial, &Dss::Ui::SerialPortViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_FALSE(serial.sendExposureCommand(false, 1, 0));
    EXPECT_EQ(statusText, QString("Serial channel is not open: Exposure"));

    ASSERT_TRUE(serial.openSerialChannel("exposure"));
    EXPECT_FALSE(serial.sendExposureCommand(false, 256, 0));
    EXPECT_EQ(statusText, QString("Exposure frame frequency code must be 0-255"));

    EXPECT_FALSE(serial.sendExposureCommand(false, 1, 0x1000000));
    EXPECT_EQ(statusText, QString("Exposure delay ticks must be 0-16777215"));
}
