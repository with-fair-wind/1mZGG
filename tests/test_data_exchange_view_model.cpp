#include <QCoreApplication>
#include <QString>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/config.h"
#include "dss/network/data_exchange.h"
#include "dss/ui/data_exchange_view_model.h"

namespace {

void ensureQCoreApplication() {
    if (QCoreApplication::instance() != nullptr) {
        return;
    }

    static int argc = 1;
    static char appName[] = "test_data_exchange_view_model";
    static char* argv[] = {appName};
    static QCoreApplication app(argc, argv);
}

}  // namespace

TEST(DataExchangeViewModel, ReportsMissingService) {
    ensureQCoreApplication();

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DataExchangeViewModel viewModel({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&viewModel, &Dss::Ui::DataExchangeViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_FALSE(viewModel.openDataExchange());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(statusText, QString("Data exchange service is not registered"));
}

TEST(DataExchangeViewModel, AppliesEndpointsWithoutOpeningUdp) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"", 0, "127.0.0.1", 51002};
    config.exchangeGdcl = {"", 0, "127.0.0.1", 51003};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(bus);
    registry.registerService<Dss::Network::DataExchange>("data_exchange", dataExchange);

    Dss::Ui::DataExchangeViewModel viewModel({.bus = bus, .registry = registry});
    int endpointChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::DataExchangeViewModel::dataExchangeEndpointsChanged,
                     [&endpointChanges] { ++endpointChanges; });

    ASSERT_TRUE(viewModel.applyDataExchangeEndpoints("127.0.0.10", 0, "127.0.0.20", 52002,
                                                     "127.0.0.11", 0, "127.0.0.21", 52003));

    EXPECT_EQ(config.exchangeGxtc.localIp, "127.0.0.10");
    EXPECT_EQ(config.exchangeGxtc.remoteIp, "127.0.0.20");
    EXPECT_EQ(config.exchangeGdcl.localIp, "127.0.0.11");
    EXPECT_EQ(config.exchangeGdcl.remotePort, 52003);
    EXPECT_EQ(viewModel.dataExchangeGxtcRemoteIp(), QString("127.0.0.20"));
    EXPECT_EQ(viewModel.dataExchangeGdclRemotePort(), 52003);
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_EQ(endpointChanges, 1);
}

TEST(DataExchangeViewModel, OpensAndClosesDataExchange) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"127.0.0.1", 0, "127.0.0.1", 51230};
    config.exchangeGdcl = {"127.0.0.1", 0, "127.0.0.1", 51231};
    config.exchange = config.exchangeGxtc;

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(bus);
    registry.registerService<Dss::Network::DataExchange>("data_exchange", dataExchange);

    Dss::Ui::DataExchangeViewModel viewModel({.bus = bus, .registry = registry});
    int openChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::DataExchangeViewModel::dataExchangeOpenChanged,
                     [&openChanges](bool) { ++openChanges; });

    ASSERT_TRUE(viewModel.openDataExchange());
    EXPECT_TRUE(dataExchange->isOpen());
    EXPECT_TRUE(viewModel.isDataExchangeOpen());

    viewModel.closeDataExchange();
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(viewModel.isDataExchangeOpen());
    EXPECT_GE(openChanges, 2);
}

TEST(DataExchangeViewModel, RejectsInvalidEndpointPorts) {
    ensureQCoreApplication();

    auto& config = Dss::Core::Config::instance().mutableCommNet();
    config.exchangeGxtc = {"", 0, "127.0.0.1", 53002};
    config.exchangeGdcl = {"", 0, "127.0.0.1", 53003};

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DataExchangeViewModel viewModel({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&viewModel, &Dss::Ui::DataExchangeViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_FALSE(
        viewModel.applyDataExchangeEndpoints("", 0, "127.0.0.2", 0, "", 0, "127.0.0.3", 53003));
    EXPECT_EQ(config.exchangeGxtc.remotePort, 53002);
    EXPECT_EQ(config.exchangeGdcl.remotePort, 53003);
    EXPECT_EQ(statusText, QString("Data exchange remote ports must be 1-65535"));

    EXPECT_FALSE(viewModel.applyDataExchangeEndpoints("", 70000, "127.0.0.2", 53002, "", 0,
                                                      "127.0.0.3", 53003));
    EXPECT_EQ(config.exchangeGxtc.localPort, 0);
    EXPECT_EQ(statusText, QString("Data exchange local ports must be 0-65535"));
}
