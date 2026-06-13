#include <QCoreApplication>
#include <QString>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "dss/comm/i_serial_channel.h"
#include "dss/core/config.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/image_sender.h"
#include "dss/ui/view_model.h"

namespace {

void ensureQCoreApplication() {
    if (QCoreApplication::instance() != nullptr) {
        return;
    }

    static int argc = 1;
    static char appName[] = "test_view_model_network";
    static char* argv[] = {appName};
    static QCoreApplication app(argc, argv);
}

/// @brief 按统一网络通道接口注册测试服务。
void registerNetworkChannel(Dss::Core::ServiceRegistry& registry, std::string_view name,
                            const std::shared_ptr<Dss::Network::INetworkChannel>& service) {
    registry.registerService<Dss::Network::INetworkChannel>(name, service);
}

/// @brief 用于 ViewModel 测试的串口通道桩，避免触碰真实串口硬件。
class RecordingSerialChannel final : public Dss::Comm::ISerialChannel {
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

    Dss::Comm::SerialConfig lastConfig{};  ///< 最近一次打开串口时收到的配置
    int openCount = 0;                     ///< 打开调用次数
    int closeCount = 0;                    ///< 关闭调用次数

private:
    std::size_t recvFrameSize_ = 0;                       ///< 固定接收帧长
    std::size_t sendFrameSize_ = 0;                       ///< 固定发送帧长
    bool isOpen_ = false;                                 ///< 当前打开状态
    Dss::Core::Status status_ = Dss::Core::Status::Init;  ///< 当前运行状态
};

/// @brief 按统一串口通道接口注册测试服务。
void registerSerialChannel(Dss::Core::ServiceRegistry& registry, std::string_view name,
                           const std::shared_ptr<Dss::Comm::ISerialChannel>& service) {
    registry.registerService<Dss::Comm::ISerialChannel>(name, service);
}

}  // namespace

TEST(ViewModelNetwork, ReportsMissingDataExchangeService) {
    ensureQCoreApplication();

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(viewModel.openDataExchange());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange service is not registered"));
}

TEST(ViewModelNetwork, ExplicitlyOpensAndClosesDataExchange) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchange.localIp = "127.0.0.1";
    config.exchange.localPort = 0;
    config.exchange.remoteIp = "127.0.0.1";
    config.exchange.remotePort = 51230;
    config.exchangeGxtc = config.exchange;
    config.exchangeGdcl = config.exchange;
    config.exchangeGdcl.remotePort = 51231;

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(bus);
    registry.registerService<Dss::Network::DataExchange>("data_exchange", dataExchange);

    Dss::Ui::ViewModel viewModel(bus, registry);
    ASSERT_FALSE(dataExchange->isOpen());
    ASSERT_FALSE(viewModel.isDataExchangeOpen());

    ASSERT_TRUE(viewModel.openDataExchange());
    EXPECT_TRUE(dataExchange->isOpen());
    EXPECT_TRUE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange UDP opened"));

    viewModel.closeDataExchange();
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange UDP closed"));
}

TEST(ViewModelNetwork, AppliesDataExchangeEndpointsWithoutOpeningUdp) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"", 0, "127.0.0.1", 51002};
    config.exchangeGdcl = {"", 0, "127.0.0.1", 51003};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(bus);
    registry.registerService<Dss::Network::DataExchange>("data_exchange", dataExchange);

    Dss::Ui::ViewModel viewModel(bus, registry);
    int endpointChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::ViewModel::dataExchangeEndpointsChanged,
                     [&endpointChanges] { ++endpointChanges; });

    ASSERT_TRUE(viewModel.applyDataExchangeEndpoints("127.0.0.10", 0, "127.0.0.20", 52002,
                                                     "127.0.0.11", 0, "127.0.0.21", 52003));

    EXPECT_EQ(config.exchangeGxtc.localIp, "127.0.0.10");
    EXPECT_EQ(config.exchangeGxtc.localPort, 0);
    EXPECT_EQ(config.exchangeGxtc.remoteIp, "127.0.0.20");
    EXPECT_EQ(config.exchangeGxtc.remotePort, 52002);
    EXPECT_EQ(config.exchangeGdcl.localIp, "127.0.0.11");
    EXPECT_EQ(config.exchangeGdcl.localPort, 0);
    EXPECT_EQ(config.exchangeGdcl.remoteIp, "127.0.0.21");
    EXPECT_EQ(config.exchangeGdcl.remotePort, 52003);
    EXPECT_EQ(viewModel.dataExchangeGxtcRemoteIp(), QString("127.0.0.20"));
    EXPECT_EQ(viewModel.dataExchangeGdclRemotePort(), 52003);
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(endpointChanges, 1);
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange endpoints applied"));
}

TEST(ViewModelNetwork, ListsEditableNetworkEndpointsFromConfig) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"", 0, "127.0.0.10", 54000};
    config.errorDiag = {"", 0, "127.0.0.11", 54001};
    config.atmos = {"", 54002, "127.0.0.12", 54003};
    config.heartbeat = {"", 54004, "127.0.0.13", 54005};
    config.exchangeGxtc = {"", 0, "127.0.0.14", 54006};
    config.exchangeGdcl = {"", 0, "127.0.0.15", 54007};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    const auto endpoints = viewModel.networkEndpointConfigs();

    ASSERT_EQ(endpoints.size(), 6U);
    EXPECT_EQ(endpoints[0].key, QString("image_sender"));
    EXPECT_EQ(endpoints[0].displayName, QString("Image Sender"));
    EXPECT_EQ(endpoints[0].remoteIp, QString("127.0.0.10"));
    EXPECT_EQ(endpoints[0].remotePort, 54000);
    EXPECT_EQ(endpoints[3].key, QString("heartbeat"));
    EXPECT_EQ(endpoints[3].localPort, 54004);
    EXPECT_EQ(endpoints[5].key, QString("exchange_gdcl"));
    EXPECT_EQ(endpoints[5].remotePort, 54007);
}

TEST(ViewModelNetwork, AppliesSingleNetworkEndpointWithoutOpeningUdp) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"", 0, "127.0.0.1", 55000};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);
    int endpointChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::ViewModel::networkEndpointsChanged,
                     [&endpointChanges] { ++endpointChanges; });

    ASSERT_TRUE(
        viewModel.applyNetworkEndpointConfig("image_sender", "127.0.0.20", 0, "127.0.0.21", 55010));

    EXPECT_EQ(config.imageSender.localIp, "127.0.0.20");
    EXPECT_EQ(config.imageSender.localPort, 0);
    EXPECT_EQ(config.imageSender.remoteIp, "127.0.0.21");
    EXPECT_EQ(config.imageSender.remotePort, 55010);
    EXPECT_EQ(endpointChanges, 1);
    EXPECT_EQ(viewModel.statusText(), QString("Network endpoint applied: Image Sender"));
}

TEST(ViewModelNetwork, ApplyingNetworkEndpointClosesOpenedService) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"127.0.0.1", 0, "127.0.0.1", 55500};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto imageSender = std::make_shared<Dss::Network::ImageSender>(bus);
    registerNetworkChannel(registry, "image_sender", imageSender);

    Dss::Ui::ViewModel viewModel(bus, registry);
    ASSERT_TRUE(viewModel.openNetworkEndpoint("image_sender"));
    ASSERT_TRUE(imageSender->isOpen());

    ASSERT_TRUE(
        viewModel.applyNetworkEndpointConfig("image_sender", "127.0.0.1", 0, "127.0.0.1", 55510));

    EXPECT_FALSE(imageSender->isOpen());
    EXPECT_FALSE(viewModel.isNetworkEndpointOpen("image_sender"));
    EXPECT_EQ(config.imageSender.remotePort, 55510);
    EXPECT_EQ(viewModel.statusText(), QString("Network endpoint applied: Image Sender"));
}

TEST(ViewModelNetwork, OpensAndClosesControllableNetworkServicesByEndpointKey) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"127.0.0.1", 0, "127.0.0.1", 57000};
    config.errorDiag = {"127.0.0.1", 0, "127.0.0.1", 57001};
    config.atmos = {"127.0.0.1", 0, "127.0.0.1", 57002};
    config.heartbeat = {"127.0.0.1", 0, "127.0.0.1", 57003};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto imageSender = std::make_shared<Dss::Network::ImageSender>(bus);
    auto errorDiagnostics = std::make_shared<Dss::Network::ErrorDiagnostics>(bus);
    auto atmosReceiver = std::make_shared<Dss::Network::AtmosReceiver>(bus);
    auto heartbeat = std::make_shared<Dss::Network::Heartbeat>(bus);
    registerNetworkChannel(registry, "image_sender", imageSender);
    registerNetworkChannel(registry, "error_diagnostics", errorDiagnostics);
    registerNetworkChannel(registry, "atmos_receiver", atmosReceiver);
    registerNetworkChannel(registry, "heartbeat", heartbeat);

    Dss::Ui::ViewModel viewModel(bus, registry);
    int serviceChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::ViewModel::networkServiceStateChanged,
                     [&serviceChanges](const QString&, bool) { ++serviceChanges; });

    ASSERT_TRUE(viewModel.openNetworkEndpoint("image_sender"));
    ASSERT_TRUE(viewModel.openNetworkEndpoint("error_diag"));
    ASSERT_TRUE(viewModel.openNetworkEndpoint("atmos"));
    ASSERT_TRUE(viewModel.openNetworkEndpoint("heartbeat"));
    EXPECT_TRUE(imageSender->isOpen());
    EXPECT_TRUE(errorDiagnostics->isOpen());
    EXPECT_TRUE(atmosReceiver->isOpen());
    EXPECT_TRUE(heartbeat->isOpen());
    EXPECT_TRUE(viewModel.isNetworkEndpointOpen("image_sender"));
    EXPECT_TRUE(viewModel.isNetworkEndpointOpen("error_diag"));
    EXPECT_TRUE(viewModel.isNetworkEndpointOpen("atmos"));
    EXPECT_TRUE(viewModel.isNetworkEndpointOpen("heartbeat"));
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("image_sender")->status(),
              Dss::Core::Status::Ok);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("error_diagnostics")->status(),
              Dss::Core::Status::Ok);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("atmos_receiver")->status(),
              Dss::Core::Status::Ok);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("heartbeat")->status(),
              Dss::Core::Status::Ok);

    viewModel.closeNetworkEndpoint("image_sender");
    viewModel.closeNetworkEndpoint("error_diag");
    viewModel.closeNetworkEndpoint("atmos");
    viewModel.closeNetworkEndpoint("heartbeat");
    EXPECT_FALSE(imageSender->isOpen());
    EXPECT_FALSE(errorDiagnostics->isOpen());
    EXPECT_FALSE(atmosReceiver->isOpen());
    EXPECT_FALSE(heartbeat->isOpen());
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("image_sender")->status(),
              Dss::Core::Status::Init);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("error_diagnostics")->status(),
              Dss::Core::Status::Init);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("atmos_receiver")->status(),
              Dss::Core::Status::Init);
    EXPECT_EQ(registry.get<Dss::Network::INetworkChannel>("heartbeat")->status(),
              Dss::Core::Status::Init);
    EXPECT_GE(serviceChanges, 8);
}

TEST(ViewModelNetwork, RejectsUnknownOrUnregisteredNetworkServiceOpen) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"127.0.0.1", 0, "127.0.0.1", 58000};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(viewModel.openNetworkEndpoint("unknown"));
    EXPECT_EQ(viewModel.statusText(), QString("Unknown network service: unknown"));

    EXPECT_FALSE(viewModel.openNetworkEndpoint("image_sender"));
    EXPECT_EQ(viewModel.statusText(), QString("Network service is not registered: Image Sender"));
    EXPECT_FALSE(viewModel.isNetworkEndpointOpen("image_sender"));
}

TEST(ViewModelNetwork, ListsSerialChannelsFromConfigWithoutOpeningHardware) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM1", 115200, 7, 2};
    config.exposurePort = {"COM2", 230400, 8, 1};
    config.masterControlPort = {"COM3", 460800, 8, 1};
    config.servoPort = {"COM4", 9600, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    registerSerialChannel(registry, "display", std::make_shared<RecordingSerialChannel>(20, 9));
    registerSerialChannel(registry, "exposure", std::make_shared<RecordingSerialChannel>(23, 8));
    registerSerialChannel(registry, "master_control",
                          std::make_shared<RecordingSerialChannel>(12, 12));
    registerSerialChannel(registry, "servo", std::make_shared<RecordingSerialChannel>(20, 14));

    Dss::Ui::ViewModel viewModel(bus, registry);

    const auto channels = viewModel.serialChannelConfigs();

    ASSERT_EQ(channels.size(), 4U);
    EXPECT_EQ(channels[0].key, QString("display"));
    EXPECT_EQ(channels[0].displayName, QString("Display"));
    EXPECT_EQ(channels[0].portName, QString("COM1"));
    EXPECT_EQ(channels[0].baudRate, 115200);
    EXPECT_EQ(channels[0].dataBits, 7);
    EXPECT_EQ(channels[0].stopBits, 2);
    EXPECT_EQ(channels[0].recvFrameSize, 20U);
    EXPECT_EQ(channels[0].sendFrameSize, 9U);
    EXPECT_TRUE(channels[0].isRegistered);
    EXPECT_FALSE(channels[0].isOpen);
    EXPECT_EQ(channels[2].key, QString("master_control"));
    EXPECT_EQ(channels[2].displayName, QString("Master Control"));
    EXPECT_EQ(channels[2].portName, QString("COM3"));
    EXPECT_EQ(channels[2].baudRate, 460800);
    EXPECT_EQ(channels[3].key, QString("servo"));
    EXPECT_EQ(channels[3].sendFrameSize, 14U);
}

TEST(ViewModelNetwork, OpensAndClosesSerialChannelsByKeyWithoutHardware) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM11", 115200, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto display = std::make_shared<RecordingSerialChannel>(20, 9);
    registerSerialChannel(registry, "display", display);

    Dss::Ui::ViewModel viewModel(bus, registry);
    int stateChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::ViewModel::serialChannelStateChanged,
                     [&stateChanges](const QString&, bool) { ++stateChanges; });

    ASSERT_TRUE(viewModel.openSerialChannel("display"));
    EXPECT_TRUE(display->isOpen());
    EXPECT_TRUE(viewModel.isSerialChannelOpen("display"));
    EXPECT_EQ(display->lastConfig.portName, "COM11");
    EXPECT_EQ(display->lastConfig.baudRate, 115200);
    EXPECT_EQ(display->openCount, 1);
    EXPECT_EQ(display->status(), Dss::Core::Status::Ok);
    EXPECT_EQ(viewModel.statusText(), QString("Serial channel opened: Display"));

    viewModel.closeSerialChannel("display");
    EXPECT_FALSE(display->isOpen());
    EXPECT_FALSE(viewModel.isSerialChannelOpen("display"));
    EXPECT_EQ(display->closeCount, 1);
    EXPECT_EQ(display->status(), Dss::Core::Status::Init);
    EXPECT_EQ(viewModel.statusText(), QString("Serial channel closed: Display"));
    EXPECT_GE(stateChanges, 2);
}

TEST(ViewModelNetwork, RejectsUnknownOrUnregisteredSerialChannelOpen) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM21", 115200, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(viewModel.openSerialChannel("unknown"));
    EXPECT_EQ(viewModel.statusText(), QString("Unknown serial channel: unknown"));

    EXPECT_FALSE(viewModel.openSerialChannel("display"));
    EXPECT_EQ(viewModel.statusText(), QString("Serial channel is not registered: Display"));
    EXPECT_FALSE(viewModel.isSerialChannelOpen("display"));
}

TEST(ViewModelNetwork, AppliesSerialChannelConfigWithoutOpeningHardware) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM31", 115200, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto display = std::make_shared<RecordingSerialChannel>(20, 9);
    registerSerialChannel(registry, "display", display);

    Dss::Ui::ViewModel viewModel(bus, registry);
    int configChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::ViewModel::serialChannelsChanged,
                     [&configChanges] { ++configChanges; });

    ASSERT_TRUE(viewModel.applySerialChannelConfig("display", " COM32 ", 57600, 7, 2));

    EXPECT_EQ(config.displayPort.portName, "COM32");
    EXPECT_EQ(config.displayPort.baudRate, 57600);
    EXPECT_EQ(config.displayPort.dataBits, 7);
    EXPECT_EQ(config.displayPort.stopBits, 2);
    EXPECT_FALSE(display->isOpen());
    EXPECT_EQ(display->openCount, 0);
    EXPECT_EQ(display->closeCount, 0);
    EXPECT_EQ(configChanges, 1);
    EXPECT_EQ(viewModel.statusText(), QString("Serial channel config applied: Display"));
}

TEST(ViewModelNetwork, ApplyingSerialChannelConfigClosesOpenedChannel) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM41", 115200, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto display = std::make_shared<RecordingSerialChannel>(20, 9);
    registerSerialChannel(registry, "display", display);

    Dss::Ui::ViewModel viewModel(bus, registry);
    ASSERT_TRUE(viewModel.openSerialChannel("display"));
    ASSERT_TRUE(display->isOpen());

    ASSERT_TRUE(viewModel.applySerialChannelConfig("display", "COM42", 230400, 8, 1));

    EXPECT_EQ(config.displayPort.portName, "COM42");
    EXPECT_EQ(config.displayPort.baudRate, 230400);
    EXPECT_FALSE(display->isOpen());
    EXPECT_FALSE(viewModel.isSerialChannelOpen("display"));
    EXPECT_EQ(display->openCount, 1);
    EXPECT_EQ(display->closeCount, 1);
    EXPECT_EQ(viewModel.statusText(), QString("Serial channel config applied: Display"));
}

TEST(ViewModelNetwork, RejectsInvalidOrUnknownSerialChannelConfig) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.displayPort = {"COM51", 115200, 8, 1};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(viewModel.applySerialChannelConfig("display", "", 57600, 8, 1));
    EXPECT_EQ(config.displayPort.portName, "COM51");
    EXPECT_EQ(viewModel.statusText(), QString("Serial port name must not be empty"));

    EXPECT_FALSE(viewModel.applySerialChannelConfig("display", "COM52", 0, 8, 1));
    EXPECT_EQ(config.displayPort.baudRate, 115200);
    EXPECT_EQ(viewModel.statusText(), QString("Serial baud rate must be 1-4000000"));

    EXPECT_FALSE(viewModel.applySerialChannelConfig("display", "COM52", 57600, 9, 1));
    EXPECT_EQ(config.displayPort.dataBits, 8);
    EXPECT_EQ(viewModel.statusText(), QString("Serial data bits must be 5-8"));

    EXPECT_FALSE(viewModel.applySerialChannelConfig("display", "COM52", 57600, 8, 3));
    EXPECT_EQ(config.displayPort.stopBits, 1);
    EXPECT_EQ(viewModel.statusText(), QString("Serial stop bits must be 1 or 2"));

    EXPECT_FALSE(viewModel.applySerialChannelConfig("unknown", "COM52", 57600, 8, 1));
    EXPECT_EQ(viewModel.statusText(), QString("Unknown serial channel: unknown"));
}

TEST(ViewModelNetwork, RejectsInvalidOrUnknownNetworkEndpoint) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.heartbeat = {"", 56000, "127.0.0.1", 56001};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(viewModel.applyNetworkEndpointConfig("heartbeat", "", -1, "127.0.0.1", 56001));
    EXPECT_EQ(config.heartbeat.localPort, 56000);
    EXPECT_EQ(viewModel.statusText(), QString("Network endpoint local port must be 0-65535"));

    EXPECT_FALSE(viewModel.applyNetworkEndpointConfig("heartbeat", "", 56000, "127.0.0.1", 0));
    EXPECT_EQ(config.heartbeat.remotePort, 56001);
    EXPECT_EQ(viewModel.statusText(), QString("Network endpoint remote port must be 1-65535"));

    EXPECT_FALSE(viewModel.applyNetworkEndpointConfig("unknown", "", 0, "127.0.0.1", 56001));
    EXPECT_EQ(viewModel.statusText(), QString("Unknown network endpoint: unknown"));
}

TEST(ViewModelNetwork, RejectsInvalidDataExchangeEndpointPorts) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"", 0, "127.0.0.1", 53002};
    config.exchangeGdcl = {"", 0, "127.0.0.1", 53003};

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_FALSE(
        viewModel.applyDataExchangeEndpoints("", 0, "127.0.0.2", 0, "", 0, "127.0.0.3", 53003));
    EXPECT_EQ(config.exchangeGxtc.remotePort, 53002);
    EXPECT_EQ(config.exchangeGdcl.remotePort, 53003);
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange remote ports must be 1-65535"));

    EXPECT_FALSE(viewModel.applyDataExchangeEndpoints("", 70000, "127.0.0.2", 53002, "", 0,
                                                      "127.0.0.3", 53003));
    EXPECT_EQ(config.exchangeGxtc.localPort, 0);
    EXPECT_EQ(viewModel.statusText(), QString("Data exchange local ports must be 0-65535"));
}

TEST(ViewModelNetwork, ShowsNetworkTransmissionErrorsInStatusText) {
    ensureQCoreApplication();

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    bus.emit(Dss::Core::NetworkTransmissionErrorEvent{
        .channel = "GXTC",
        .message = "GXTC UDP channel is not open or send failed",
        .attemptedBytes = 32,
    });

    EXPECT_EQ(viewModel.statusText(),
              QString("Network send failed (GXTC): GXTC UDP channel is not open or send failed"));
}
