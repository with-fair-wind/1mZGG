#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/camera/i_camera_controller.h"
#include "dss/acquisition/source/frame_source_coordinator.h"
#include "dss/acquisition/source/i_frame_source.h"
#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/app/application_context.h"
#include "dss/app/track_result_data_exchange_bridge.h"
#include "dss/comm/channel/serial_worker_base.h"
#include "dss/comm/port/i_serial_channel.h"
#include "dss/comm/port/serial_command_interfaces.h"
#include "dss/core/event/events.h"
#include "dss/core/logger.h"
#include "dss/network/endpoint/atmos_receiver.h"
#include "dss/network/endpoint/data_exchange.h"
#include "dss/network/endpoint/error_diagnostics.h"
#include "dss/network/endpoint/heartbeat.h"
#include "dss/network/endpoint/image_sender.h"
#include "dss/network/transport/i_network_channel.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/storage/backend/i_storage_backend.h"
#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"

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

/// @brief 暴露字节注入与解码计数的测试工作线程,用于验证串口流式重同步。
class ResyncSerialWorker final : public Dss::Comm::SerialWorkerBase {
public:
    using SerialWorkerBase::SerialWorkerBase;

    void feedBytes(std::span<const std::uint8_t> bytes) { processReceivedBytes(bytes); }
    [[nodiscard]] int decodedCount() const { return m_decoded; }

protected:
    [[nodiscard]] auto recvFrameSize() const -> std::size_t override { return 6U; }
    [[nodiscard]] auto sendFrameSize() const -> std::size_t override { return 0U; }
    [[nodiscard]] auto channelName() const -> std::string_view override { return "test"; }

    void decodeFrame(std::span<const std::uint8_t> data) override {
        ++m_decoded;
        m_lastPayload.assign(data.begin() + 1, data.end() - 1);
    }
    void encodeFrame(std::span<std::uint8_t> /*buffer*/) override {}

private:
    int m_decoded = 0;
    std::vector<std::uint8_t> m_lastPayload;
};

[[nodiscard]] auto makeTestFrame(std::array<std::uint8_t, 4> payload) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> frame{Dss::Comm::FrameCodec::HEADER};
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(Dss::Comm::FrameCodec::TAIL);
    return frame;
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
    const auto frameSource =
        context.registry().get<Dss::Acquisition::FrameSourceCoordinator>("frame_source");

    ASSERT_NE(camera, nullptr);
    EXPECT_FALSE(camera->isOpen());
    ASSERT_NE(frameSource, nullptr);
    EXPECT_EQ(frameSource->activeMode(), Dss::Acquisition::FrameSourceMode::Replay);
    EXPECT_FALSE(frameSource->isRunning());

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

TEST(ApplicationContextServices, SerialWorkerReportsBackgroundOpenFailure) {
    Dss::App::ApplicationContext::MessageBus bus;
    DecodeErrorPublishingSerialWorker worker(bus);
    Dss::Core::SerialConfig config{};
    config.portName = "DSS_NON_EXISTENT_SERIAL_PORT";
    config.baudRate = 115200;
    config.dataBits = 8;

    const auto result = worker.open(config);

    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(worker.isOpen());
    EXPECT_EQ(worker.status(), Dss::Core::Status::Error);
    EXPECT_NE(result.error().find(config.portName), std::string::npos);
    worker.close();
    EXPECT_EQ(worker.status(), Dss::Core::Status::Init);
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

TEST(ApplicationContextServices, RoutesReadyImagesToImageSender) {
    Dss::App::ApplicationContext context;
    context.registerCommunicationServices();
    const auto imageSender = context.registry().get<Dss::Network::ImageSender>("image_sender");
    ASSERT_NE(imageSender, nullptr);
    ASSERT_TRUE(
        imageSender
            ->open(
                {.localIp = "127.0.0.1", .localPort = 0, .remoteIp = "127.0.0.1", .remotePort = 9})
            .has_value());

    std::promise<Dss::Core::ImageSendCompletedEvent> completedPromise;
    auto completedFuture = completedPromise.get_future();
    auto connection = context.bus().subscribe<Dss::Core::ImageSendCompletedEvent>(
        [&completedPromise](const auto& event) { completedPromise.set_value(event); });
    auto image = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1, 2, 3, 4});

    context.bus().emit(Dss::Core::ImageReadyForSendEvent{
        .frameSeq = 88U,
        .width = 2U,
        .height = 2U,
        .image = std::move(image),
    });

    ASSERT_EQ(completedFuture.wait_for(std::chrono::seconds{2}), std::future_status::ready);
    EXPECT_EQ(completedFuture.get().frameSeq, 88U);
    imageSender->close();
}
TEST(ApplicationContextServices, ConfiguresRotatingLoggerAfterLoadingConfig) {
    const auto dir = std::filesystem::temp_directory_path() / "dss_context_logger_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto configPath = dir / "config.json";
    const auto logPath = dir / "logs" / "dss.log";
    {
        std::ofstream output(configPath);
        output << "{\"logging\":{\"enabled\":true,\"filePath\":\"" << logPath.generic_string()
               << "\",\"maxFileSizeBytes\":4096,\"maxFiles\":2}}";
    }

    Dss::App::ApplicationContext context;
    context.wireLogger();
    ASSERT_TRUE(context.loadConfig(configPath).has_value());
    Dss::Core::Logger::instance().info("context rotating logger ready");
    Dss::Core::Logger::instance().flush();

    ASSERT_TRUE(std::filesystem::exists(logPath));
    EXPECT_NE(readAllText(logPath).find("context rotating logger ready"), std::string::npos);
}

TEST(ApplicationContextServices, ShutdownClearsRegistryAndIsIdempotent) {
    Dss::App::ApplicationContext context;
    context.registerCommunicationServices();
    EXPECT_NE(context.registry().tryGet<Dss::Processing::ImageProcessor>("image_processor"),
              nullptr);

    context.shutdown();

    EXPECT_EQ(context.registry().tryGet<Dss::Processing::ImageProcessor>("image_processor"),
              nullptr);
    // 幂等:重复显式调用 + 析构兜底调用均不应崩溃
    context.shutdown();
}

TEST(ApplicationContextServices, ShutdownStopsRunningWorkersBeforeDestruction) {
    Dss::App::ApplicationContext context;
    context.registerCommunicationServices();
    {
        const auto trackDataStorage =
            context.registry().get<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
        const auto dir = tempContextTrackStorageDir();
        ASSERT_TRUE(trackDataStorage->init(dir).has_value());
        ASSERT_TRUE(trackDataStorage->start().has_value());
        ASSERT_TRUE(trackDataStorage->isRunning());
    }  // 释放外部副本,仅 registry 持有——模拟 ViewModel 不缓存服务引用

    context.shutdown();  // registry.clear 析构 trackDataStorage → stop+join worker
    // 到达此处即证明关机路径无死锁、无崩溃
}

TEST(SerialResync, DecodesConsecutiveValidFrames) {
    Dss::App::ApplicationContext::MessageBus bus;
    ResyncSerialWorker worker(bus);
    const auto frame = makeTestFrame({1, 2, 3, 4});
    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), frame.begin(), frame.end());
    stream.insert(stream.end(), frame.begin(), frame.end());
    worker.feedBytes({stream.data(), stream.size()});
    EXPECT_EQ(worker.decodedCount(), 2);
}

TEST(SerialResync, RecoversFromGarbageByteBetweenFrames) {
    Dss::App::ApplicationContext::MessageBus bus;
    ResyncSerialWorker worker(bus);
    const auto frame = makeTestFrame({1, 2, 3, 4});
    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), frame.begin(), frame.end());
    stream.push_back(0xAA);  // 失步字节
    stream.insert(stream.end(), frame.begin(), frame.end());
    worker.feedBytes({stream.data(), stream.size()});
    EXPECT_EQ(worker.decodedCount(), 2);  // 两帧均恢复解码
}

TEST(SerialResync, RecoversFromTailMismatchAndReportsError) {
    Dss::App::ApplicationContext::MessageBus bus;
    std::vector<Dss::Core::SerialFrameErrorEvent> errors;
    [[maybe_unused]] auto connection = bus.subscribe<Dss::Core::SerialFrameErrorEvent>(
        [&](const Dss::Core::SerialFrameErrorEvent& event) { errors.push_back(event); });
    ResyncSerialWorker worker(bus);
    const std::vector<std::uint8_t> badFrame{Dss::Comm::FrameCodec::HEADER, 5, 6, 7, 8, 0xBB};
    const auto goodFrame = makeTestFrame({9, 10, 11, 12});
    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), badFrame.begin(), badFrame.end());
    stream.insert(stream.end(), goodFrame.begin(), goodFrame.end());
    worker.feedBytes({stream.data(), stream.size()});
    EXPECT_EQ(worker.decodedCount(), 1);  // 坏帧被跳过,好帧解码
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().observedTail, 0xBB);
}

TEST(SerialResync, BuffersPartialFrameAcrossFeeds) {
    Dss::App::ApplicationContext::MessageBus bus;
    ResyncSerialWorker worker(bus);
    const auto frame = makeTestFrame({1, 2, 3, 4});
    worker.feedBytes({frame.data(), 3});  // 前半(不足一帧)
    EXPECT_EQ(worker.decodedCount(), 0);
    worker.feedBytes({frame.data() + 3, frame.size() - 3});  // 后半
    EXPECT_EQ(worker.decodedCount(), 1);  // 拼成完整帧后解码
}

TEST(SerialResync, ClearsAccumulatorOnCloseBeforeNextSession) {
    Dss::App::ApplicationContext::MessageBus bus;
    std::vector<Dss::Core::SerialFrameErrorEvent> errors;
    [[maybe_unused]] auto conn = bus.subscribe<Dss::Core::SerialFrameErrorEvent>(
        [&](const Dss::Core::SerialFrameErrorEvent& event) { errors.push_back(event); });

    ResyncSerialWorker worker(bus);
    const std::vector<std::uint8_t> staleHalf{Dss::Comm::FrameCodec::HEADER, 1, 2, 3, 4};
    worker.feedBytes({staleHalf.data(), staleHalf.size()});  // 旧会话残留半帧(差1字节满帧)
    ASSERT_EQ(worker.decodedCount(), 0);

    worker.close();  // closeLocked 清 m_rxAccumulator(模拟重连前清理)

    const auto frame = makeTestFrame({10, 11, 12, 13});
    worker.feedBytes({frame.data(), frame.size()});  // 新会话完整帧
    EXPECT_EQ(worker.decodedCount(), 1);
    // close 清了 → 新帧干净解码,无重同步错误;若残留则会触发 TailMismatch 事件
    EXPECT_TRUE(errors.empty());
}
