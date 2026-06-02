#include <gtest/gtest.h>

#include "dss/acquisition/i_camera_controller.h"
#include "dss/app/application_context.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/image_sender.h"
#include "dss/storage/i_storage_backend.h"

TEST(ApplicationContextServices, RegistersCommunicationServicesWithoutOpeningPorts) {
    Dss::App::ApplicationContext context;

    context.registerCommunicationServices();

    const auto display = context.registry().get<Dss::Comm::ISerialChannel>("display");
    const auto exposure = context.registry().get<Dss::Comm::ISerialChannel>("exposure");
    const auto master = context.registry().get<Dss::Comm::ISerialChannel>("master_control");
    const auto servo = context.registry().get<Dss::Comm::ISerialChannel>("servo");

    ASSERT_NE(display, nullptr);
    ASSERT_NE(exposure, nullptr);
    ASSERT_NE(master, nullptr);
    ASSERT_NE(servo, nullptr);

    EXPECT_FALSE(display->isOpen());
    EXPECT_FALSE(exposure->isOpen());
    EXPECT_FALSE(master->isOpen());
    EXPECT_FALSE(servo->isOpen());
    EXPECT_EQ(display->recvFrameSize(), 20U);
    EXPECT_EQ(exposure->recvFrameSize(), 23U);
    EXPECT_EQ(master->recvFrameSize(), 30U);
    EXPECT_EQ(servo->sendFrameSize(), 14U);

    const auto imageSender = context.registry().get<Dss::Network::ImageSender>("image_sender");
    const auto heartbeat = context.registry().get<Dss::Network::Heartbeat>("heartbeat");
    const auto errorDiagnostics =
        context.registry().get<Dss::Network::ErrorDiagnostics>("error_diagnostics");
    const auto dataExchange = context.registry().get<Dss::Network::DataExchange>("data_exchange");
    const auto atmosReceiver =
        context.registry().get<Dss::Network::AtmosReceiver>("atmos_receiver");

    ASSERT_NE(imageSender, nullptr);
    ASSERT_NE(heartbeat, nullptr);
    ASSERT_NE(errorDiagnostics, nullptr);
    ASSERT_NE(dataExchange, nullptr);
    ASSERT_NE(atmosReceiver, nullptr);

    EXPECT_FALSE(imageSender->isOpen());
    EXPECT_FALSE(heartbeat->isOpen());
    EXPECT_FALSE(errorDiagnostics->isOpen());
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(atmosReceiver->isOpen());

    const auto camera = context.registry().get<Dss::Acquisition::ICameraController>("camera");

    ASSERT_NE(camera, nullptr);
    EXPECT_FALSE(camera->isOpen());

    const auto imageStorage =
        context.registry().get<Dss::Storage::IStorageBackend>("image_storage");

    ASSERT_NE(imageStorage, nullptr);
    EXPECT_FALSE(imageStorage->isReady());

    const auto trackDataStorage =
        context.registry().get<Dss::Storage::IStorageBackend>("track_data_storage");

    ASSERT_NE(trackDataStorage, nullptr);
    EXPECT_FALSE(trackDataStorage->isReady());
}
