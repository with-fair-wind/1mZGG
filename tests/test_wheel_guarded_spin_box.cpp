#include <QApplication>
#include <QDoubleSpinBox>
#include <QPoint>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QWheelEvent>
#include <QWidget>
#include <memory>

#include <gtest/gtest.h>

#include "dss/core/service_registry.h"
#include "dss/ui/main_window.h"
#include "dss/ui/wheel_guarded_spin_box.h"

namespace {

auto ensureApplication() -> QApplication& {
    static int argc = 1;
    static char appName[] = "test_wheel_guarded_spin_box";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QApplication> app;

    if (QApplication::instance() == nullptr) {
        app = std::make_unique<QApplication>(argc, argv);
    }
    return *qobject_cast<QApplication*>(QApplication::instance());
}

auto findTabByText(QTabWidget& tabs, const QString& text) -> QWidget* {
    for (int index = 0; index < tabs.count(); ++index) {
        if (tabs.tabText(index) == text) {
            return tabs.widget(index);
        }
    }
    return nullptr;
}

void sendWheel(QWidget& target, int angleDeltaY) {
    const QPoint globalPosition = target.mapToGlobal(target.rect().center());
    for (auto* receiver = &target; receiver != nullptr; receiver = receiver->parentWidget()) {
        const QPointF localPosition{receiver->mapFromGlobal(globalPosition)};
        QWheelEvent event(localPosition, QPointF{globalPosition}, QPoint{}, QPoint{0, angleDeltaY},
                          Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(receiver, &event);
        if (event.isAccepted()) {
            break;
        }
    }
    QApplication::processEvents();
}

struct ScrollHarness {
    QScrollArea area;
    QWidget* content = new QWidget;
    Dss::Ui::WheelGuardedSpinBox* spin = new Dss::Ui::WheelGuardedSpinBox(content);

    ScrollHarness() {
        area.resize(240, 120);
        content->setMinimumSize(220, 600);
        spin->setGeometry(20, 20, 120, 30);
        spin->setRange(0, 100);
        spin->setValue(50);
        area.setWidget(content);
        area.show();
        QApplication::processEvents();
    }
};

struct DoubleScrollHarness {
    QScrollArea area;
    QWidget* content = new QWidget;
    Dss::Ui::WheelGuardedDoubleSpinBox* spin = new Dss::Ui::WheelGuardedDoubleSpinBox(content);

    DoubleScrollHarness() {
        area.resize(240, 120);
        content->setMinimumSize(220, 600);
        spin->setGeometry(20, 20, 120, 30);
        spin->setRange(0.0, 100.0);
        spin->setSingleStep(0.25);
        spin->setValue(50.0);
        area.setWidget(content);
        area.show();
        QApplication::processEvents();
    }
};

}  // namespace

TEST(WheelGuardedSpinBox, DoesNotAcquireFocusFromWheelInput) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::WheelGuardedSpinBox spin;

    EXPECT_EQ(spin.focusPolicy(), Qt::StrongFocus);
}

TEST(WheelGuardedDoubleSpinBox, DoesNotAcquireFocusFromWheelInput) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::WheelGuardedDoubleSpinBox spin;

    EXPECT_EQ(spin.focusPolicy(), Qt::StrongFocus);
}

TEST(WheelGuardedSpinBox, UnfocusedIntegerEditorPassesWheelToScrollArea) {
    auto& app = ensureApplication();
    (void)app;
    ScrollHarness harness;
    ASSERT_GT(harness.area.verticalScrollBar()->maximum(), 0);
    harness.area.setFocus(Qt::OtherFocusReason);
    harness.spin->clearFocus();
    ASSERT_FALSE(harness.spin->hasFocus());
    harness.area.verticalScrollBar()->setValue(0);
    const auto valueBefore = harness.spin->value();

    sendWheel(*harness.spin, -120);

    EXPECT_EQ(harness.spin->value(), valueBefore);
    EXPECT_GT(harness.area.verticalScrollBar()->value(), 0);
}

TEST(WheelGuardedSpinBox, FocusedIntegerEditorRetainsNativeWheelBehavior) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::WheelGuardedSpinBox spin;
    spin.setRange(0, 100);
    spin.setValue(50);
    spin.show();
    spin.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_TRUE(spin.hasFocus());

    sendWheel(spin, 120);

    EXPECT_EQ(spin.value(), 51);
}

TEST(WheelGuardedDoubleSpinBox, UnfocusedDoubleEditorPassesWheelToScrollArea) {
    auto& app = ensureApplication();
    (void)app;
    DoubleScrollHarness harness;
    ASSERT_GT(harness.area.verticalScrollBar()->maximum(), 0);
    harness.area.setFocus(Qt::OtherFocusReason);
    harness.spin->clearFocus();
    ASSERT_FALSE(harness.spin->hasFocus());
    harness.area.verticalScrollBar()->setValue(0);
    const auto valueBefore = harness.spin->value();

    sendWheel(*harness.spin, -120);

    EXPECT_DOUBLE_EQ(harness.spin->value(), valueBefore);
    EXPECT_GT(harness.area.verticalScrollBar()->value(), 0);
}

TEST(WheelGuardedDoubleSpinBox, FocusedDoubleEditorRetainsNativeWheelBehavior) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::WheelGuardedDoubleSpinBox spin;
    spin.setRange(0.0, 100.0);
    spin.setSingleStep(0.25);
    spin.setValue(50.0);
    spin.show();
    spin.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_TRUE(spin.hasFocus());

    sendWheel(spin, 120);

    EXPECT_DOUBLE_EQ(spin.value(), 50.25);
}

TEST(WheelGuardedSpinBox, AllProductionPagesUseGuardedSpinBoxes) {
    auto& app = ensureApplication();
    (void)app;
    Dss::Ui::MainViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::MainViewModel viewModel(bus, registry);
    Dss::Ui::MainWindow window(viewModel);
    auto* tabs = window.findChild<QTabWidget*>();
    if (tabs == nullptr) {
        GTEST_SKIP() << "Ela navigation does not expose the fallback QTabWidget.";
    }

    for (const auto& pageName :
         {QString{"Display"}, QString{"Communication"}, QString{"Settings"}}) {
        auto* page = findTabByText(*tabs, pageName);
        ASSERT_NE(page, nullptr);
        const auto integers = page->findChildren<QSpinBox*>();
        const auto doubles = page->findChildren<QDoubleSpinBox*>();
        EXPECT_GT(integers.size() + doubles.size(), 0) << pageName.toStdString();
        for (auto* spin : integers) {
            EXPECT_NE(dynamic_cast<Dss::Ui::WheelGuardedSpinBox*>(spin), nullptr)
                << pageName.toStdString();
        }
        for (auto* spin : doubles) {
            EXPECT_NE(dynamic_cast<Dss::Ui::WheelGuardedDoubleSpinBox*>(spin), nullptr)
                << pageName.toStdString();
        }
    }
}
