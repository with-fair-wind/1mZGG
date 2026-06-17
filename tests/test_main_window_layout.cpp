#include <QApplication>
#include <QCheckBox>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/service_registry.h"
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
