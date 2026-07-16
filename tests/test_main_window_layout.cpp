#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTimer>
#include <algorithm>
#include <array>
#include <memory>

#include <gtest/gtest.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "dss/core/service/service_registry.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/ui/widget/main_window.h"

namespace {

/// @brief 创建或复用测试进程中的 QApplication 实例。
auto ensureApplication() -> QApplication& {
    static int argc = 1;
    static char appName[] = "test_main_window_layout";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QApplication> app;

    if (QApplication::instance() == nullptr) {
        QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        app = std::make_unique<QApplication>(argc, argv);
        QApplication::setOrganizationName("DSS_QT_Tests");
        QApplication::setApplicationName("test_main_window_layout");
    }
    return *qobject_cast<QApplication*>(QApplication::instance());
}

/// @brief 按稳定对象名查找主窗口中的功能页 Dock。
auto findDock(Dss::Ui::MainWindow& window, const char* objectName) -> QDockWidget* {
    return window.findChild<QDockWidget*>(objectName);
}

/// @brief 按标签文本查找原生 Dock 标签栏。
auto findDockTabBar(Dss::Ui::MainWindow& window, const QString& title) -> QTabBar* {
    const auto tabBars = window.findChildren<QTabBar*>();
    for (auto* tabBar : tabBars) {
        for (int index = 0; index < tabBar->count(); ++index) {
            if (tabBar->tabText(index) == title) {
                return tabBar;
            }
        }
    }
    return nullptr;
}

/// @brief 清除布局测试使用的隔离设置，避免测试间共享 Dock 状态。
void clearWorkspaceSettings() {
    QSettings settings;
    settings.remove("ui/main_window");
    settings.sync();
}

void processEventsFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

TEST(MainWindowLayout, CreatesNativeDockWorkspaceForAllPages) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    constexpr std::array dockNames{"control_dock",       "display_dock",  "analysis_dock",
                                   "communication_dock", "settings_dock", "logs_dock"};
    for (const auto* dockName : dockNames) {
        auto* dock = findDock(window, dockName);
        ASSERT_NE(dock, nullptr) << dockName;
        EXPECT_NE(dock->widget(), nullptr);
        EXPECT_EQ(dock->features(), QDockWidget::DockWidgetMovable |
                                        QDockWidget::DockWidgetFloatable |
                                        QDockWidget::DockWidgetClosable);
        EXPECT_EQ(dock->allowedAreas(), Qt::AllDockWidgetAreas);
    }
}

TEST(MainWindowLayout, TabifiedDockUsesSingleTitleSurfaceAndDoubleClickFloats) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* displayDock = findDock(window, "display_dock");
    auto* tabBar = findDockTabBar(window, "Display");
    ASSERT_NE(displayDock, nullptr);
    ASSERT_NE(tabBar, nullptr);
    ASSERT_NE(displayDock->titleBarWidget(), nullptr);
    EXPECT_EQ(displayDock->titleBarWidget()->objectName(), "hidden_tabified_dock_title_bar");
    EXPECT_EQ(displayDock->titleBarWidget()->height(), 0);
    EXPECT_EQ(tabBar->contextMenuPolicy(), Qt::CustomContextMenu);

    int displayTabIndex = -1;
    for (int index = 0; index < tabBar->count(); ++index) {
        if (tabBar->tabText(index) == "Display") {
            displayTabIndex = index;
            break;
        }
    }
    ASSERT_GE(displayTabIndex, 0);

    tabBar->tabBarDoubleClicked(displayTabIndex);
    QApplication::processEvents();

    EXPECT_TRUE(displayDock->isFloating());
    EXPECT_EQ(displayDock->titleBarWidget(), nullptr);
    EXPECT_FALSE(displayDock->windowFlags().testFlag(Qt::FramelessWindowHint));
    EXPECT_TRUE(displayDock->windowFlags().testFlag(Qt::WindowTitleHint));
#ifdef Q_OS_WIN
    const auto nativeStyle =
        GetWindowLongPtrW(reinterpret_cast<HWND>(displayDock->winId()), GWL_STYLE);
    EXPECT_NE(nativeStyle & WS_CAPTION, 0);
#endif
    clearWorkspaceSettings();
}

TEST(MainWindowLayout, SeparatelyDockedPageKeepsNativeTitleBar) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* displayDock = findDock(window, "display_dock");
    ASSERT_NE(displayDock, nullptr);

    window.removeDockWidget(displayDock);
    window.addDockWidget(Qt::RightDockWidgetArea, displayDock);
    displayDock->show();
    QApplication::processEvents();

    EXPECT_TRUE(window.tabifiedDockWidgets(displayDock).isEmpty());
    EXPECT_EQ(displayDock->titleBarWidget(), nullptr);
    clearWorkspaceSettings();
}

TEST(MainWindowLayout, InitialControlFloatingGeometryIsUsable) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* controlDock = findDock(window, "control_dock");
    auto* tabBar = findDockTabBar(window, "Control");
    ASSERT_NE(controlDock, nullptr);
    ASSERT_NE(tabBar, nullptr);

    int controlTabIndex = -1;
    for (int index = 0; index < tabBar->count(); ++index) {
        if (tabBar->tabText(index) == "Control") {
            controlTabIndex = index;
            break;
        }
    }
    ASSERT_GE(controlTabIndex, 0);
    tabBar->tabBarDoubleClicked(controlTabIndex);
    QApplication::processEvents();

    ASSERT_NE(window.screen(), nullptr);
    const auto availableGeometry = window.screen()->availableGeometry();
    EXPECT_GE(controlDock->width(),
              std::min(window.width() / 2, availableGeometry.width() * 4 / 5));
    EXPECT_GE(controlDock->height(),
              std::min(window.height() / 2, availableGeometry.height() * 4 / 5));
    EXPECT_TRUE(availableGeometry.contains(controlDock->frameGeometry().center()));
}

TEST(MainWindowLayout, RefloatingDockKeepsUserAdjustedSize) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* controlDock = findDock(window, "control_dock");
    auto* displayDock = findDock(window, "display_dock");
    auto* tabBar = findDockTabBar(window, "Control");
    ASSERT_NE(controlDock, nullptr);
    ASSERT_NE(displayDock, nullptr);
    ASSERT_NE(tabBar, nullptr);

    int controlTabIndex = tabBar->count();
    for (int index = 0; index < tabBar->count(); ++index) {
        if (tabBar->tabText(index) == "Control") {
            controlTabIndex = index;
            break;
        }
    }
    ASSERT_LT(controlTabIndex, tabBar->count());
    tabBar->tabBarDoubleClicked(controlTabIndex);
    QApplication::processEvents();

    constexpr QSize userSize{900, 620};
    controlDock->resize(userSize);
    controlDock->setFloating(false);
    window.tabifyDockWidget(displayDock, controlDock);
    controlDock->raise();
    QApplication::processEvents();

    tabBar = findDockTabBar(window, "Control");
    ASSERT_NE(tabBar, nullptr);
    controlTabIndex = tabBar->count();
    for (int index = 0; index < tabBar->count(); ++index) {
        if (tabBar->tabText(index) == "Control") {
            controlTabIndex = index;
            break;
        }
    }
    ASSERT_LT(controlTabIndex, tabBar->count());
    tabBar->tabBarDoubleClicked(controlTabIndex);
    QApplication::processEvents();

    EXPECT_EQ(controlDock->size(), userSize);
}

TEST(MainWindowLayout, CommunicationPageUsesScrollableContainer) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);

    auto* communicationDock = findDock(window, "communication_dock");
    ASSERT_NE(communicationDock, nullptr);
    auto* scrollArea = qobject_cast<QScrollArea*>(communicationDock->widget());
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

    auto* displayDock = findDock(window, "display_dock");
    ASSERT_NE(displayDock, nullptr);
    auto* displayPage = displayDock->widget();
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

TEST(MainWindowLayout, DisplayDockFloatsWithoutReplacingDisplayPage) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* displayDock = findDock(window, "display_dock");
    ASSERT_NE(displayDock, nullptr);
    auto* displayPage = displayDock->widget();
    ASSERT_NE(displayPage, nullptr);
    auto* displayWidget = displayPage->findChild<QWidget*>("main_image_display");
    ASSERT_NE(displayWidget, nullptr);

    displayDock->setFloating(true);
    QApplication::processEvents();

    EXPECT_TRUE(displayDock->isFloating());
    EXPECT_EQ(displayDock->widget(), displayPage);
    EXPECT_EQ(displayPage->findChild<QWidget*>("main_image_display"), displayWidget);

    displayDock->setFloating(false);
    QApplication::processEvents();

    EXPECT_FALSE(displayDock->isFloating());
    EXPECT_EQ(displayDock->widget(), displayPage);
    EXPECT_EQ(displayPage->findChild<QWidget*>("main_image_display"), displayWidget);
}

TEST(MainWindowLayout, ViewMenuRestoresHiddenDockAndResetsDefaultLayout) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    Dss::Ui::MainWindow window(mainViewModel);
    window.show();
    QApplication::processEvents();

    auto* viewMenu = window.findChild<QMenu*>("view_menu");
    auto* resetAction = window.findChild<QAction*>("reset_workspace_layout");
    auto* displayDock = findDock(window, "display_dock");
    auto* logsDock = findDock(window, "logs_dock");
    ASSERT_NE(viewMenu, nullptr);
    ASSERT_NE(resetAction, nullptr);
    ASSERT_NE(displayDock, nullptr);
    ASSERT_NE(logsDock, nullptr);

    EXPECT_TRUE(logsDock->close());
    QApplication::processEvents();
    EXPECT_FALSE(logsDock->isVisible());
    EXPECT_FALSE(logsDock->toggleViewAction()->isChecked());

    logsDock->toggleViewAction()->trigger();
    QApplication::processEvents();
    EXPECT_TRUE(logsDock->isVisible());

    displayDock->setFloating(true);
    EXPECT_TRUE(logsDock->close());
    resetAction->trigger();
    QApplication::processEvents();

    for (const auto* dockName : {"control_dock", "display_dock", "analysis_dock",
                                 "communication_dock", "settings_dock", "logs_dock"}) {
        auto* dock = findDock(window, dockName);
        ASSERT_NE(dock, nullptr);
        EXPECT_TRUE(dock->isVisible()) << dockName;
        EXPECT_FALSE(dock->isFloating()) << dockName;
    }
    EXPECT_EQ(window.tabifiedDockWidgets(displayDock).size(), 5);
    clearWorkspaceSettings();
}

TEST(MainWindowLayout, RestoresSavedDockVisibilityAndFloatingState) {
    auto& app = ensureApplication();
    (void)app;
    clearWorkspaceSettings();

    {
        Dss::Ui::MainViewModel::MessageBus bus;
        Dss::Core::ServiceRegistry registry;
        Dss::Ui::MainViewModel mainViewModel(bus, registry);
        Dss::Ui::MainWindow window(mainViewModel);
        window.show();
        QApplication::processEvents();

        auto* displayDock = findDock(window, "display_dock");
        auto* logsDock = findDock(window, "logs_dock");
        ASSERT_NE(displayDock, nullptr);
        ASSERT_NE(logsDock, nullptr);
        displayDock->setFloating(true);
        EXPECT_TRUE(logsDock->close());
        QApplication::processEvents();
        window.close();
        QApplication::processEvents();
    }

    {
        Dss::Ui::MainViewModel::MessageBus bus;
        Dss::Core::ServiceRegistry registry;
        Dss::Ui::MainViewModel mainViewModel(bus, registry);
        Dss::Ui::MainWindow window(mainViewModel);
        window.show();
        QApplication::processEvents();

        auto* displayDock = findDock(window, "display_dock");
        auto* logsDock = findDock(window, "logs_dock");
        ASSERT_NE(displayDock, nullptr);
        ASSERT_NE(logsDock, nullptr);
        EXPECT_TRUE(displayDock->isFloating());
        EXPECT_FALSE(logsDock->isVisible());
    }
    clearWorkspaceSettings();
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

    auto* displayDock = findDock(window, "display_dock");
    ASSERT_NE(displayDock, nullptr);
    auto* displayPage = displayDock->widget();
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

    auto* settingsDock = findDock(window, "settings_dock");
    ASSERT_NE(settingsDock, nullptr);
    auto* settingsPage = settingsDock->widget();
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

    auto* logsDock = findDock(window, "logs_dock");
    ASSERT_NE(logsDock, nullptr);
    auto* logPage = logsDock->widget();
    ASSERT_NE(logPage, nullptr);
    EXPECT_NE(logPage->findChild<QLineEdit*>("log_search"), nullptr);
    EXPECT_NE(logPage->findChild<QPushButton*>("log_export"), nullptr);
}
