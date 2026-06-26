#include <QCoreApplication>
#include <QString>
#include <expected>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "dss/core/config.h"
#include "dss/network/i_network_channel.h"
#include "dss/ui/network_view_model.h"

namespace {

void ensureQCoreApplication() {
    if (QCoreApplication::instance() != nullptr) {
        return;
    }

    static int argc = 1;
    static char appName[] = "test_network_view_model";
    static char* argv[] = {appName};
    static QCoreApplication app(argc, argv);
}

/// @brief 网络 ViewModel 测试用通道桩，避免绑定真实 UDP 端口。
class RecordingNetworkChannel final : public Dss::Network::INetworkChannel {
public:
    /// @brief 记录打开参数并切换为已打开状态。
    auto open(const Dss::Core::UdpEndpointConfig& config)
        -> std::expected<void, std::string> override {
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

    /// @brief 查询测试通道是否已打开。
    [[nodiscard]] bool isOpen() const override {
        return isOpen_;
    }

    /// @brief 获取测试通道运行状态。
    [[nodiscard]] auto status() const -> Dss::Core::Status override {
        return status_;
    }

    Dss::Core::UdpEndpointConfig lastConfig{};  ///< 最近一次打开时收到的端点配置。
    int openCount = 0;                          ///< 打开调用次数。
    int closeCount = 0;                         ///< 关闭调用次数。

private:
    bool isOpen_ = false;                                 ///< 当前打开状态。
    Dss::Core::Status status_ = Dss::Core::Status::Init;  ///< 当前运行状态。
};

}  // namespace

TEST(NetworkViewModel, ListsEditableEndpointsFromConfig) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"", 0, "127.0.0.10", 54000};
    config.errorDiag = {"", 0, "127.0.0.11", 54001};
    config.atmos = {"", 54002, "127.0.0.12", 54003};
    config.heartbeat = {"", 54004, "127.0.0.13", 54005};
    config.exchangeGxtc = {"", 0, "127.0.0.14", 54006};
    config.exchangeGdcl = {"", 0, "127.0.0.15", 54007};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::NetworkViewModel viewModel({.bus = bus, .registry = registry});

    const auto endpoints = viewModel.networkEndpointConfigs();

    ASSERT_EQ(endpoints.size(), 6U);
    EXPECT_EQ(endpoints[0].key, QString("image_sender"));
    EXPECT_EQ(endpoints[0].displayName, QString("Image Sender"));
    EXPECT_TRUE(endpoints[0].canOpen);
    EXPECT_EQ(endpoints[0].remoteIp, QString("127.0.0.10"));
    EXPECT_EQ(endpoints[3].key, QString("heartbeat"));
    EXPECT_EQ(endpoints[3].localPort, 54004);
    EXPECT_EQ(endpoints[5].key, QString("exchange_gdcl"));
    EXPECT_FALSE(endpoints[5].canOpen);
    EXPECT_EQ(endpoints[5].remotePort, 54007);
}

TEST(NetworkViewModel, AppliesEndpointAndClosesOpenedService) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.imageSender = {"127.0.0.1", 0, "127.0.0.1", 55000};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto imageSender = std::make_shared<RecordingNetworkChannel>();
    registry.registerService<Dss::Network::INetworkChannel>("image_sender", imageSender);

    Dss::Ui::NetworkViewModel viewModel({.bus = bus, .registry = registry});
    int endpointChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::NetworkViewModel::networkEndpointsChanged,
                     [&endpointChanges] { ++endpointChanges; });

    ASSERT_TRUE(viewModel.openNetworkEndpoint("image_sender"));
    ASSERT_TRUE(imageSender->isOpen());

    ASSERT_TRUE(viewModel.applyNetworkEndpointConfig("image_sender", " 127.0.0.20 ", 0,
                                                     "127.0.0.21", 55010));

    EXPECT_EQ(config.imageSender.localIp, "127.0.0.20");
    EXPECT_EQ(config.imageSender.remoteIp, "127.0.0.21");
    EXPECT_EQ(config.imageSender.remotePort, 55010);
    EXPECT_FALSE(imageSender->isOpen());
    EXPECT_FALSE(viewModel.isNetworkEndpointOpen("image_sender"));
    EXPECT_GE(endpointChanges, 3);
}

TEST(NetworkViewModel, EmitsDataExchangeSignalsWhenEditingExchangeEndpoint) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"", 0, "127.0.0.1", 56001};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::NetworkViewModel viewModel({.bus = bus, .registry = registry});
    int dataExchangeEndpointChanges = 0;
    int dataExchangeReconfigs = 0;
    QObject::connect(&viewModel, &Dss::Ui::NetworkViewModel::dataExchangeEndpointsChanged,
                     [&dataExchangeEndpointChanges] { ++dataExchangeEndpointChanges; });
    QObject::connect(&viewModel, &Dss::Ui::NetworkViewModel::dataExchangeEndpointEdited,
                     [&dataExchangeReconfigs] { ++dataExchangeReconfigs; });

    ASSERT_TRUE(viewModel.applyNetworkEndpointConfig("exchange_gxtc", "", 0, "127.0.0.22", 56022));

    EXPECT_EQ(config.exchangeGxtc.remoteIp, "127.0.0.22");
    EXPECT_EQ(config.exchange.remoteIp, "127.0.0.22");
    EXPECT_EQ(dataExchangeEndpointChanges, 1);
    EXPECT_EQ(dataExchangeReconfigs, 1);
}

TEST(NetworkViewModel, RejectsInvalidEndpointConfig) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.heartbeat = {"", 56000, "127.0.0.1", 56001};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::NetworkViewModel viewModel({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&viewModel, &Dss::Ui::NetworkViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_FALSE(viewModel.applyNetworkEndpointConfig("heartbeat", "", -1, "127.0.0.1", 56001));
    EXPECT_EQ(config.heartbeat.localPort, 56000);
    EXPECT_EQ(statusText, QString("Network endpoint local port must be 0-65535"));

    EXPECT_FALSE(viewModel.applyNetworkEndpointConfig("heartbeat", "", 56000, "127.0.0.1", 0));
    EXPECT_EQ(config.heartbeat.remotePort, 56001);
    EXPECT_EQ(statusText, QString("Network endpoint remote port must be 1-65535"));
}
