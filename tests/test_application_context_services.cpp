#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "dss/acquisition/i_camera_controller.h"
#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/app/application_context.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/image_sender.h"
#include "dss/processing/image_processor.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"

namespace {

[[nodiscard]] auto tempContextTrackStorageDir() -> std::filesystem::path {
    auto dir =
        std::filesystem::temp_directory_path() / "dss_application_context_track_storage_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto readAllText(const std::filesystem::path& path) -> std::string {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] auto trackEvent() -> Dss::Core::TrackResultEvent {
    Dss::Core::MeasuredBlob blob{};
    blob.centroid = Dss::Core::Vec2f{3072.0F, 3072.0F};
    blob.area = 5.0F;

    Dss::Core::TargetFrameInfo frame{};
    frame.frameSeq = 9;
    frame.timestamp = {.year = 2026, .month = 6, .day = 4, .hour = 1, .minute = 2, .second = 3};
    frame.fovCenterAe = Dss::Core::Vec2f{1.0F, 2.0F};
    frame.exposureTime = 0.02F;
    frame.measuredBlob = blob;
    frame.valid = true;

    Dss::Core::TargetInfo target{};
    target.targetId = "manual";
    target.living = true;
    target.frameInfos.push_back(frame);

    Dss::Core::TrackResultEvent event{};
    event.frameSeq = 9;
    event.targets.push_back(std::move(target));
    return event;
}

}  // namespace

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

    const auto imageProcessor =
        context.registry().get<Dss::Processing::ImageProcessor>("image_processor");
    const auto replaySource =
        context.registry().get<Dss::Acquisition::IFrameSource>("replay_source");
    const auto imageSequenceSource =
        context.registry().get<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");

    ASSERT_NE(imageProcessor, nullptr);
    ASSERT_NE(replaySource, nullptr);
    ASSERT_NE(imageSequenceSource, nullptr);
    EXPECT_FALSE(imageProcessor->isRunning());
    EXPECT_FALSE(replaySource->isRunning());
    EXPECT_EQ(replaySource->frameWidth(), 0U);
    EXPECT_EQ(replaySource->frameHeight(), 0U);
    EXPECT_EQ(imageSequenceSource->frameCount(), 0U);

    const auto imageStorage =
        context.registry().get<Dss::Storage::IStorageBackend>("image_storage");
    const auto localImageStorage =
        context.registry().get<Dss::Storage::LocalImageStorageBackend>("image_storage");

    ASSERT_NE(imageStorage, nullptr);
    ASSERT_NE(localImageStorage, nullptr);
    EXPECT_FALSE(imageStorage->isReady());
    EXPECT_FALSE(localImageStorage->isRunning());

    const auto trackDataStorage =
        context.registry().get<Dss::Storage::IStorageBackend>("track_data_storage");
    const auto concreteTrackDataStorage =
        context.registry().get<Dss::Storage::TrackDataStorageBackend>("track_data_storage");

    ASSERT_NE(trackDataStorage, nullptr);
    ASSERT_NE(concreteTrackDataStorage, nullptr);
    EXPECT_FALSE(trackDataStorage->isReady());
    EXPECT_FALSE(concreteTrackDataStorage->isRunning());
}

TEST(ApplicationContextServices, RoutesTrackResultsToTrackDataStorage) {
    Dss::App::ApplicationContext context;
    context.registerCommunicationServices();

    const auto trackDataStorage =
        context.registry().get<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    const auto dir = tempContextTrackStorageDir();
    ASSERT_TRUE(trackDataStorage->init(dir).has_value());
    ASSERT_TRUE(trackDataStorage->start().has_value());

    context.bus().emit(trackEvent());

    trackDataStorage->stop();

    ASSERT_TRUE(std::filesystem::exists(trackDataStorage->outputPath()));
    EXPECT_FALSE(readAllText(trackDataStorage->outputPath()).empty());
}
