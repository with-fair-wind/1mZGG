#include <QCoreApplication>
#include <QEventLoop>
#include <QString>
#include <QThread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#include "dss/ui/view_model/main_view_model.h"

namespace {

using namespace std::chrono_literals;

/// @brief 创建或复用测试进程中的 QCoreApplication 实例。
auto ensureQCoreApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_main_view_model";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

template <typename T>
auto waitForReady(QCoreApplication& app, std::future<T>& future,
                  std::chrono::milliseconds timeout = 2s) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (future.wait_for(0ms) == std::future_status::ready) {
            return true;
        }
        app.processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(5ms);
    }
    return future.wait_for(0ms) == std::future_status::ready;
}

}  // namespace

TEST(MainViewModel, AggregatesChildStatusText) {
    auto& app = ensureQCoreApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel viewModel(bus, registry);
    QString statusText;
    QObject::connect(&viewModel, &Dss::Ui::MainViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    viewModel.processing().setProcessingMode(static_cast<int>(Dss::Core::ProcessingMode::Direct));

    EXPECT_EQ(statusText, QString("Image processor is not registered"));
    EXPECT_EQ(viewModel.statusText(), QString("Image processor is not registered"));
}

TEST(MainViewModel, MasterControlCoordinatesExposureAndTrackingMode) {
    auto& app = ensureQCoreApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel viewModel(bus, registry);
    int exposureChanges = 0;
    QObject::connect(&viewModel, &Dss::Ui::MainViewModel::exposureChanged,
                     [&exposureChanges](double) { ++exposureChanges; });

    bus.emit(Dss::Core::MasterControlEvent{
        .exposure = 12.5,
        .trackMode = static_cast<int>(Dss::Core::TrackMode::Geo),
        .save = false,
        .grab = false,
    });

    EXPECT_DOUBLE_EQ(viewModel.exposure(), 12.5);
    EXPECT_EQ(exposureChanges, 1);
    EXPECT_EQ(viewModel.tracking().trackMode(), static_cast<int>(Dss::Core::TrackMode::Geo));
}

TEST(MainViewModel, MasterControlEventFromWorkerThreadUpdatesOnObjectThread) {
    auto& app = ensureQCoreApplication();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel viewModel(bus, registry);

    std::promise<QThread*> signalThreadPromise;
    auto signalThreadFuture = signalThreadPromise.get_future();
    QObject::connect(&viewModel, &Dss::Ui::MainViewModel::exposureChanged,
                     [&signalThreadPromise](double) {
                         signalThreadPromise.set_value(QThread::currentThread());
                     });

    std::jthread worker([&bus] {
        bus.emit(Dss::Core::MasterControlEvent{
            .exposure = 22.5,
            .trackMode = static_cast<int>(Dss::Core::TrackMode::Leo),
            .save = false,
            .grab = false,
        });
    });
    worker.join();

    ASSERT_TRUE(waitForReady(app, signalThreadFuture));
    EXPECT_EQ(signalThreadFuture.get(), viewModel.thread());
    EXPECT_DOUBLE_EQ(viewModel.exposure(), 22.5);
    EXPECT_EQ(viewModel.tracking().trackMode(), static_cast<int>(Dss::Core::TrackMode::Leo));
}
TEST(MainViewModel, MasterControlCreatesTaskSpecificStorageSession) {
    auto& app = ensureQCoreApplication();
    (void)app;

    const auto root = std::filesystem::temp_directory_path() / "dss_main_view_model_session";
    std::filesystem::remove_all(root);
    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto imageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(root / "images");
    auto trackStorage = std::make_shared<Dss::Storage::TrackDataStorageBackend>(root / "tracks");
    registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage", imageStorage);
    registry.registerService<Dss::Storage::TrackDataStorageBackend>("track_data_storage",
                                                                    trackStorage);
    Dss::Ui::MainViewModel viewModel(bus, registry);

    bus.emit(Dss::Core::MasterControlEvent{
        .save = true,
        .targetId = 42,
        .taskId = 7,
        .start = {.hour = 8, .minute = 30},
        .end = {.hour = 9, .minute = 45},
    });

    ASSERT_TRUE(viewModel.storage().isSaving());
    EXPECT_NE(imageStorage->sessionPath().filename().string().find("_42_0.BMP"), std::string::npos);
    EXPECT_NE(trackStorage->outputPath().filename().string().find("_42_0.GAE"), std::string::npos);
    const auto imiPath =
        imageStorage->baseDir() / (imageStorage->sessionPath().stem().string() + ".IMI");
    std::ifstream imi(imiPath);
    ASSERT_TRUE(imi);
    const std::string content{std::istreambuf_iterator<char>{imi}, {}};
    EXPECT_NE(content.find("C 7\n"), std::string::npos);
    imi.close();

    bus.emit(Dss::Core::MasterControlEvent{
        .save = true,
        .targetId = 43,
        .taskId = 8,
        .start = {.hour = 10},
        .end = {.hour = 11},
    });
    EXPECT_NE(imageStorage->sessionPath().filename().string().find("_43_0.BMP"), std::string::npos);
    EXPECT_NE(trackStorage->outputPath().filename().string().find("_43_0.GAE"), std::string::npos);

    bus.emit(Dss::Core::MasterControlEvent{.save = false});
    std::filesystem::remove_all(root);
}
