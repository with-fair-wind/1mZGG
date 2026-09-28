#include <QCoreApplication>
#include <QTemporaryDir>
#include <chrono>
#include <filesystem>
#include <future>
#include <latch>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#include "dss/ui/view_model/storage_view_model.h"
#include "dss/ui/view_model/view_model_context.h"
namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_storage_view_model";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

[[nodiscard]] auto tempStorageViewModelDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_storage_view_model_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

TEST(StorageViewModel, SavingToggleControlsImageAndTrackStorage) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::UiServiceContext::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    const auto dir = tempStorageViewModelDir();
    auto imageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(dir / "images");
    auto trackStorage = std::make_shared<Dss::Storage::TrackDataStorageBackend>(dir / "tracks");

    registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage", imageStorage);
    registry.registerService<Dss::Storage::TrackDataStorageBackend>("track_data_storage",
                                                                    trackStorage);

    Dss::Ui::StorageViewModel storage(Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    const Dss::Storage::ImageStorageNaming naming{
        .startTime = "20260620083000",
        .endTime = "20260620094500",
        .taskId = "7",
        .targetId = "42",
        .observatoryId = "999",
        .imageFormat = "raw",
    };
    storage.startSaving(naming);

    EXPECT_TRUE(storage.isSaving());
    EXPECT_TRUE(imageStorage->isRunning());
    EXPECT_TRUE(trackStorage->isRunning());
    EXPECT_TRUE(imageStorage->hasSession());
    EXPECT_TRUE(std::filesystem::exists(imageStorage->sessionPath()));
    EXPECT_EQ(imageStorage->sessionPath().filename(), "20260620083000_42_999.BMP");
    EXPECT_EQ(trackStorage->outputPath().extension(), ".GAE");
    EXPECT_EQ(trackStorage->outputPath().filename(), "20260620083000_42_999.GAE");

    storage.stopSaving();

    EXPECT_FALSE(storage.isSaving());
    EXPECT_FALSE(imageStorage->isRunning());
    EXPECT_FALSE(trackStorage->isRunning());
}

TEST(StorageViewModel, StopReturnsWhileWriterIsStillDraining) {
    (void)ensureApplication();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto dir = std::filesystem::path(directory.path().toStdWString());
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto backend = std::make_shared<Dss::Storage::LocalImageStorageBackend>(dir);
    backend->setBus(&bus);
    registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage", backend);
    Dss::Ui::StorageViewModel vm({.bus = bus, .registry = registry});
    std::latch entered{1}, release{1};
    auto connection = bus.subscribe<Dss::Core::StorageWriteErrorEvent>([&](const auto&) {
        entered.count_down();
        release.wait();
    });
    vm.startSaving();
    ASSERT_TRUE(vm.isSaving());
    std::ofstream(dir / "blocked") << "not a directory";
    Dss::Storage::RawImageMetadata metadata{};
    metadata.width = 1;
    metadata.height = 1;
    ASSERT_TRUE(
        backend->enqueueRawFrame("blocked/frame.raw", metadata, std::vector<std::uint16_t>{1}));
    entered.wait();
    std::promise<void> returned;
    auto future = returned.get_future();
    bool responsive = false;
    std::jthread releaser([&] {
        responsive = future.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
        release.count_down();
    });
    vm.stopSaving();
    returned.set_value();
    EXPECT_FALSE(vm.isSaving());
    EXPECT_TRUE(vm.isStopping());
    EXPECT_FALSE(backend->isRunning());
    releaser.join();
    vm.shutdown();
    EXPECT_TRUE(responsive);
}

TEST(StorageViewModel, PendingSessionKeepsLatestRequestAndShutdownPreventsRestart) {
    auto& app = ensureApplication();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto backend = std::make_shared<Dss::Storage::LocalImageStorageBackend>(
        std::filesystem::path(directory.path().toStdWString()));
    registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage", backend);
    Dss::Ui::StorageViewModel vm({.bus = bus, .registry = registry});
    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "first";
    naming.targetId = "1";
    vm.startSaving(naming);
    ASSERT_TRUE(vm.isSaving());
    vm.stopSaving();
    naming.startTime = "second";
    vm.startSaving(naming);
    naming.startTime = "third";
    vm.startSaving(naming);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (vm.isStopping() && std::chrono::steady_clock::now() < deadline) {
        app.processEvents();
        std::this_thread::yield();
    }
    EXPECT_TRUE(vm.isSaving());
    EXPECT_TRUE(backend->sessionPath().filename().string().starts_with("third"));
    vm.stopSaving();
    naming.startTime = "never";
    vm.startSaving(naming);
    vm.shutdown();
    app.processEvents();
    EXPECT_FALSE(backend->isRunning());
    EXPECT_TRUE(backend->sessionPath().filename().string().starts_with("third"));
}
