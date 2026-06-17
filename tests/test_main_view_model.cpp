#include <QCoreApplication>
#include <QString>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/ui/main_view_model.h"

namespace {

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
