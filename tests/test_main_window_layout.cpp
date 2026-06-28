#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/service_registry.h"
#include "dss/processing/image_processor.h"
#include "dss/ui/main_window.h"

namespace {

/// @brief 创建或复用测试进程中的 QApplication 实例。
auto ensureApplication() -> QApplication& {
    static int argc = 1;
    static char appName[] = "test_main_window_layout";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QApplication> app;

    if (QApplication::instance() == nullptr) {
        app = std::make_unique<QApplication>(argc, argv);
    }
    return *qobject_cast<QApplication*>(QApplication::instance());
}

/// @brief 按标签文本查找回退导航中的页面控件。
auto findTabByText(QTabWidget& tabs, const QString& text) -> QWidget* {
    for (int index = 0; index < tabs.count(); ++index) {
        if (tabs.tabText(index) == text) {
            return tabs.widget(index);
        }
    }
    return nullptr;
}

void processEventsFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

TEST(MainWindowLayout, CommunicationPageUsesScrollableContainer) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }

    auto* communicationPage = findTabByText(*tabs, "Communication");
    ASSERT_NE(communicationPage, nullptr);

    auto* scrollArea = qobject_cast<QScrollArea*>(communicationPage);
    ASSERT_NE(scrollArea, nullptr);
    EXPECT_TRUE(scrollArea->widgetResizable());
    EXPECT_NE(scrollArea->widget(), nullptr);
    EXPECT_NE(scrollArea->findChild<QWidget*>("serial_channels_panel"), nullptr);
    EXPECT_NE(scrollArea->findChild<QWidget*>("serial_commands_panel"), nullptr);
    EXPECT_NE(scrollArea->findChild<QWidget*>("network_endpoints_panel"), nullptr);
}

TEST(MainWindowLayout, DisplayPageExposesLegacyStretchControls) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }

    auto* displayPage = findTabByText(*tabs, "Display");
    ASSERT_NE(displayPage, nullptr);

    EXPECT_NE(displayPage->findChild<QCheckBox*>("display_stretch_auto"), nullptr);
    auto* lowSlider = displayPage->findChild<QSlider*>("display_stretch_low_slider");
    auto* highSlider = displayPage->findChild<QSlider*>("display_stretch_high_slider");
    ASSERT_NE(lowSlider, nullptr);
    ASSERT_NE(highSlider, nullptr);
    EXPECT_EQ(lowSlider->minimum(), 0);
    EXPECT_EQ(lowSlider->maximum(), 16383);
    EXPECT_EQ(highSlider->minimum(), 1);
    EXPECT_EQ(highSlider->maximum(), 16384);
    EXPECT_NE(displayPage->findChild<QSpinBox*>("display_stretch_low_spin"), nullptr);
    EXPECT_NE(displayPage->findChild<QSpinBox*>("display_stretch_high_spin"), nullptr);
}
TEST(MainWindowLayout, DisplayStretchSliderThrottlesRapidPreviewUpdates) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }

    auto* displayPage = findTabByText(*tabs, "Display");
    ASSERT_NE(displayPage, nullptr);
    auto* autoStretch = displayPage->findChild<QCheckBox*>("display_stretch_auto");
    auto* lowSlider = displayPage->findChild<QSlider*>("display_stretch_low_slider");
    ASSERT_NE(autoStretch, nullptr);
    ASSERT_NE(lowSlider, nullptr);

    int changedCount = 0;
    int lastLow = mainViewModel.display().displayStretchLow();
    const auto connection = QObject::connect(&mainViewModel.display(),
                                             &Dss::Ui::DisplayViewModel::displayStretchChanged,
                                             [&changedCount, &lastLow](bool, int low, int) {
                                                 ++changedCount;
                                                 lastLow = low;
                                             });

    autoStretch->setChecked(false);
    QApplication::processEvents();
    processEventsFor(80);
    changedCount = 0;

    lowSlider->setValue(1001);
    QApplication::processEvents();
    EXPECT_EQ(changedCount, 1);
    EXPECT_EQ(lastLow, 1001);

    lowSlider->setValue(1002);
    QApplication::processEvents();
    EXPECT_EQ(changedCount, 1);
    EXPECT_EQ(lastLow, 1001);

    processEventsFor(80);
    EXPECT_EQ(changedCount, 2);
    EXPECT_EQ(lastLow, 1002);

    processEventsFor(80);
    lowSlider->setSliderDown(true);
    lowSlider->setValue(1003);
    QApplication::processEvents();
    EXPECT_EQ(changedCount, 3);
    EXPECT_EQ(lastLow, 1003);

    lowSlider->setValue(1004);
    QApplication::processEvents();
    EXPECT_EQ(changedCount, 3);
    EXPECT_EQ(lastLow, 1003);

    lowSlider->setSliderDown(false);
    QApplication::processEvents();
    EXPECT_EQ(changedCount, 4);
    EXPECT_EQ(lastLow, 1004);

    QObject::disconnect(connection);
}
TEST(MainWindowLayout, SettingsPageExposesPersistentProductionControls) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }
    auto* settingsPage = findTabByText(*tabs, "Settings");
    ASSERT_NE(settingsPage, nullptr);
    EXPECT_NE(settingsPage->findChild<QLineEdit*>("settings_data_root"), nullptr);
    EXPECT_NE(settingsPage->findChild<QLineEdit*>("settings_log_path"), nullptr);
    EXPECT_NE(settingsPage->findChild<QPushButton*>("settings_save"), nullptr);
}
TEST(MainWindowLayout, LogPageExposesSearchAndExportControls) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }
    auto* logPage = findTabByText(*tabs, "Logs");
    ASSERT_NE(logPage, nullptr);
    EXPECT_NE(logPage->findChild<QLineEdit*>("log_search"), nullptr);
    EXPECT_NE(logPage->findChild<QPushButton*>("log_export"), nullptr);
}
