#include <QCoreApplication>
#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"
#include "dss/ui/storage_view_model.h"
#include "dss/ui/view_model_context.h"

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

    Dss::Ui::StorageViewModel storage(Dss::Ui::UiServiceContext{.bus = bus,
                                                                .registry = registry});

    storage.startSaving();

    EXPECT_TRUE(storage.isSaving());
    EXPECT_TRUE(imageStorage->isRunning());
    EXPECT_TRUE(trackStorage->isRunning());

    storage.stopSaving();

    EXPECT_FALSE(storage.isSaving());
    EXPECT_FALSE(imageStorage->isRunning());
    EXPECT_FALSE(trackStorage->isRunning());
}
