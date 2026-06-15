#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/i_camera_controller.h"
#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/app/application_context.h"
#include "dss/app/track_result_data_exchange_bridge.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/comm/serial_command_interfaces.h"
#include "dss/comm/serial_worker_base.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/i_network_channel.h"
#include "dss/network/image_sender.h"
#include "dss/processing/image_processor.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"

namespace {

/// @brief 暴露串口字段级诊断发布能力的测试工作线程。
class DecodeErrorPublishingSerialWorker final : public Dss::Comm::SerialWorkerBase {
public:
    using SerialWorkerBase::SerialWorkerBase;

    /// @brief 触发一次字段级解码错误事件。
    void publishForTest(std::string_view field, std::string_view message, std::size_t byteOffset,
                        uint64_t rawValue) {
        publishDecodeError(field, message, byteOffset, rawValue);
    }

protected:
    [[nodiscard]] auto recvFrameSize() const -> size_t override {
        return 20U;
    }

    [[nodiscard]] auto sendFrameSize() const -> size_t override {
        return 0U;
    }

    [[nodiscard]] auto channelName() const -> std::string_view override {
        return "display";
    }

    void decodeFrame(std::span<const uint8_t> /*data*/) override {}
    void encodeFrame(std::span<uint8_t> /*buffer*/) override {}
};

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
    const auto exposureCommand =
        context.registry().get<Dss::Comm::IExposureCommandPort>("exposure");
    const auto masterStatus =
        context.registry().get<Dss::Comm::IMasterControlStatusPort>("master_control");
    const auto servoCorrection = context.registry().get<Dss::Comm::IServoCorrectionPort>("servo");

    ASSERT_NE(display, nullptr);
    ASSERT_NE(exposure, nullptr);
    ASSERT_NE(master, nullptr);
    ASSERT_NE(servo, nullptr);
    ASSERT_NE(exposureCommand, nullptr);
    ASSERT_NE(masterStatus, nullptr);
    ASSERT_NE(servoCorrection, nullptr);

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
    const auto imageSenderChannel =
        context.registry().get<Dss::Network::INetworkChannel>("image_sender");
    const auto heartbeatChannel =
        context.registry().get<Dss::Network::INetworkChannel>("heartbeat");
    const auto errorDiagnosticsChannel =
        context.registry().get<Dss::Network::INetworkChannel>("error_diagnostics");
    const auto atmosReceiverChannel =
        context.registry().get<Dss::Network::INetworkChannel>("atmos_receiver");
    const auto trackResultDataExchangeBridge =
        context.registry().get<Dss::App::TrackResultDataExchangeBridge>(
            "track_result_data_exchange_bridge");

    ASSERT_NE(imageSender, nullptr);
    ASSERT_NE(heartbeat, nullptr);
    ASSERT_NE(errorDiagnostics, nullptr);
    ASSERT_NE(dataExchange, nullptr);
    ASSERT_NE(atmosReceiver, nullptr);
    ASSERT_NE(imageSenderChannel, nullptr);
    ASSERT_NE(heartbeatChannel, nullptr);
    ASSERT_NE(errorDiagnosticsChannel, nullptr);
    ASSERT_NE(atmosReceiverChannel, nullptr);
    ASSERT_NE(trackResultDataExchangeBridge, nullptr);

    EXPECT_FALSE(imageSender->isOpen());
    EXPECT_FALSE(heartbeat->isOpen());
    EXPECT_FALSE(errorDiagnostics->isOpen());
    EXPECT_FALSE(dataExchange->isOpen());
    EXPECT_FALSE(atmosReceiver->isOpen());
    EXPECT_FALSE(imageSenderChannel->isOpen());
    EXPECT_FALSE(heartbeatChannel->isOpen());
    EXPECT_FALSE(errorDiagnosticsChannel->isOpen());
    EXPECT_FALSE(atmosReceiverChannel->isOpen());
    EXPECT_EQ(imageSenderChannel->status(), Dss::Core::Status::Init);
    EXPECT_EQ(heartbeatChannel->status(), Dss::Core::Status::Init);
    EXPECT_EQ(errorDiagnosticsChannel->status(), Dss::Core::Status::Init);
    EXPECT_EQ(atmosReceiverChannel->status(), Dss::Core::Status::Init);

    context.bus().emit(trackEvent());
    EXPECT_FALSE(dataExchange->isOpen());

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

TEST(ApplicationContextServices, SerialChannelPublishesDecodeErrorForInvalidField) {
    Dss::App::ApplicationContext::MessageBus bus;
    std::vector<Dss::Core::SerialDecodeErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::SerialDecodeErrorEvent>(
        [&errors](const Dss::Core::SerialDecodeErrorEvent& event) { errors.push_back(event); });

    DecodeErrorPublishingSerialWorker worker(bus);
    worker.publishForTest("timestamp.month", "timestamp.month has invalid BCD value 0x1A", 2U,
                          0x1AU);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().channel, "display");
    EXPECT_EQ(errors.front().field, "timestamp.month");
    EXPECT_EQ(errors.front().byteOffset, 2U);
    EXPECT_EQ(errors.front().rawValue, 0x1AU);
    EXPECT_EQ(errors.front().message, "timestamp.month has invalid BCD value 0x1A");
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
