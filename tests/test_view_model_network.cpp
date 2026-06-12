#include <QCoreApplication>
#include <QString>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/config.h"
#include "dss/network/data_exchange.h"
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
