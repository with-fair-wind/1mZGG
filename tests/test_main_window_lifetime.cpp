#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QSlider>
#include <QTimer>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "dss/core/service/service_registry.h"
#include "dss/ui/widget/main_window.h"

namespace {
/// @brief 创建独立于原生 Dock 布局断言的窗口生命周期测试应用。
auto ensureApplication() -> QApplication& {
    static int argc = 1;
    static char appName[] = "test_main_window_lifetime";
    static char* argv[] = {appName, nullptr};
    static auto app = std::make_unique<QApplication>(argc, argv);
    return *app;
}

/// @brief 驱动事件直到超出拉伸节流周期，覆盖排队更新与定时器销毁。
void processEventsFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
}  // namespace

TEST(MainWindowLifetime, ViewModelsCanPublishAfterWindowDestruction) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    {
        Dss::Ui::MainWindow window(mainViewModel);
    }

    // 模拟窗口已销毁、服务和 ViewModel 尚在完成当前会话的顺序。
    mainViewModel.replay().replayFrameCountChanged(8);
    mainViewModel.replay().replayCurrentFrameChanged(3);
    mainViewModel.replay().replayBusyChanged(false);
    mainViewModel.display().displayStretchChanged(false, 100, 1000);
    mainViewModel.display().imageStatsUpdated(1.0, 20.0, 10.0, 2.0);
    QApplication::processEvents();

    Dss::Ui::MainWindow replacement(mainViewModel);
    auto* progress = replacement.findChild<QSlider*>("replay_progress");
    ASSERT_NE(progress, nullptr);
    mainViewModel.replay().replayFrameCountChanged(8);
    mainViewModel.replay().replayCurrentFrameChanged(3);
    EXPECT_EQ(progress->maximum(), 7);
    EXPECT_EQ(progress->value(), 2);
}

TEST(MainWindowLifetime, QueuedViewModelUpdatesAndStretchTimerSurviveWindowTeardown) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel mainViewModel(bus, registry);
    {
        Dss::Ui::MainWindow window(mainViewModel);
        auto* autoStretch = window.findChild<QCheckBox*>("display_stretch_auto");
        auto* lowSlider = window.findChild<QSlider*>("display_stretch_low_slider");
        ASSERT_NE(autoStretch, nullptr);
        ASSERT_NE(lowSlider, nullptr);
        autoStretch->setChecked(false);
        lowSlider->setValue(100);
        lowSlider->setValue(101);
        auto* previewTimer = window.findChild<QTimer*>("display_stretch_preview_timer");
        ASSERT_NE(previewTimer, nullptr);
        // 显式保留一个待执行预览，避免依赖控制更新恰好快于节流周期。
        previewTimer->start(40);
        ASSERT_TRUE(previewTimer->isActive());

        {
            // 先在服务线程发布，使带 context 的控件更新排入 UI 队列。
            std::jthread publisher([&mainViewModel] {
                mainViewModel.replay().replayFrameCountChanged(8);
                mainViewModel.display().displayStretchChanged(false, 100, 1000);
                mainViewModel.display().imageStatsUpdated(1.0, 20.0, 10.0, 2.0);
            });
        }
        ASSERT_TRUE(QMetaObject::invokeMethod(
            &mainViewModel.replay(),
            [&mainViewModel] {
                mainViewModel.replay().replayBusyChanged(true);
                mainViewModel.replay().replayFrameCountChanged(8);
                mainViewModel.display().displayStretchChanged(false, 100, 1000);
                mainViewModel.display().imageStatsUpdated(1.0, 20.0, 10.0, 2.0);
            },
            Qt::QueuedConnection));
    }
    // 先释放页面，再投递已排队更新及超过拉伸预览周期的事件。
    processEventsFor(80);
}
