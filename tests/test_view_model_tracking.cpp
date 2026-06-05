#include <QCoreApplication>
#include <QPointF>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/processing/image_processor.h"
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"
#include "dss/ui/view_model.h"

using namespace std::chrono_literals;

namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_view_model_tracking";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

[[nodiscard]] auto tempViewModelStorageDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_view_model_tracking_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

TEST(ViewModelTracking, SelectTargetEnablesManualTrackingOnImageProcessor) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::ViewModel viewModel(bus, registry);

    std::promise<Dss::Core::TrackResultEvent> trackPromise;
    auto trackFuture = trackPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::TrackResultEvent>(
        [&trackPromise](Dss::Core::TrackResultEvent event) {
            trackPromise.set_value(std::move(event));
        });

    viewModel.selectTarget(QPointF{120.0, 80.0});

    EXPECT_EQ(viewModel.trackMode(), static_cast<int>(Dss::Core::TrackMode::Manual));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Manual);

    Dss::Processing::FramePacket packet{};
    packet.frameSeq = 12;
    packet.width = 4;
    packet.height = 4;
    packet.displayImage = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    packet.metadata.pointingAe = Dss::Core::Vec2f{1.0F, 2.0F};
    packet.metadata.frameFrequency = 10.0F;

    processor->start();
    ASSERT_TRUE(processor->submitFrame(std::move(packet)));

    ASSERT_EQ(trackFuture.wait_for(2s), std::future_status::ready);
    processor->stop();

    const auto event = trackFuture.get();
    ASSERT_EQ(event.targets.size(), 1U);
    EXPECT_EQ(event.frameSeq, 12U);
    EXPECT_EQ(event.targets.front().targetId, "manual");
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.x, 120.0F);
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.y, 80.0F);
}

TEST(ViewModelTracking, SetTrackModeConfiguresNonManualTrackingStrategies) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::ViewModel viewModel(bus, registry);

    viewModel.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Geo));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Geo);

    viewModel.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Leo));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Leo);

    viewModel.setTrackMode(static_cast<int>(Dss::Core::TrackMode::SpaceCatalog));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::SpaceCatalog);

    viewModel.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Init));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Init);
}

TEST(ViewModelTracking, SavingToggleControlsImageAndTrackStorage) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    const auto dir = tempViewModelStorageDir();
    auto imageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(dir / "images");
    auto trackStorage = std::make_shared<Dss::Storage::TrackDataStorageBackend>(dir / "tracks");

    registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage", imageStorage);
    registry.registerService<Dss::Storage::TrackDataStorageBackend>("track_data_storage",
                                                                    trackStorage);

    Dss::Ui::ViewModel viewModel(bus, registry);

    viewModel.startSaving();

    EXPECT_TRUE(viewModel.isSaving());
    EXPECT_TRUE(imageStorage->isRunning());
    EXPECT_TRUE(trackStorage->isRunning());

    viewModel.stopSaving();

    EXPECT_FALSE(viewModel.isSaving());
    EXPECT_FALSE(imageStorage->isRunning());
    EXPECT_FALSE(trackStorage->isRunning());
}
