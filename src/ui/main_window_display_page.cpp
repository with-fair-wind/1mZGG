#include <QCheckBox>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "dss/ui/display/image_display.h"
#include "dss/ui/support/app_event.h"
#ifdef DSS_HAS_OPENGL_WIDGETS
#include "dss/ui/display/gpu_image_display.h"
#endif
#include "dss/ui/widget/main_window.h"
#include "dss/ui/widget/wheel_guarded_spin_box.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaSlider.h>
#endif

namespace Dss::Ui {

namespace {

/// @brief 将关闭操作转换为显示页还原请求的独立窗口。
class DisplayFloatingWindow final : public QMainWindow {
public:
    explicit DisplayFloatingWindow(std::function<void()> restorePage, QWidget* parent)
        : QMainWindow(parent, Qt::Window), m_restorePage(std::move(restorePage)) {}

protected:
    void closeEvent(QCloseEvent* event) override {
        event->ignore();
        if (m_restorePage) {
            m_restorePage();
        }
    }

private:
    std::function<void()> m_restorePage;  ///< 关闭浮窗时执行的页面还原操作。
};

}  // namespace

void MainWindow::setupDisplayPage() {
    m_displayPage = new QWidget;
    m_displayPage->setObjectName("display_page");
    auto* hostLayout = new QVBoxLayout(m_displayPage);
    hostLayout->setContentsMargins(0, 0, 0, 0);

    m_displayPageContent = new QWidget(m_displayPage);
    m_displayPageContent->setObjectName("display_page_content");
    hostLayout->addWidget(m_displayPageContent, 1);

    auto* contentLayout = new QVBoxLayout(m_displayPageContent);
    auto* pageTools = new QHBoxLayout;
    pageTools->addStretch();
    m_displayDetachButton = new QToolButton(m_displayPageContent);
    m_displayDetachButton->setObjectName("display_detach_button");
    m_displayDetachButton->setAutoRaise(true);
    m_displayDetachButton->setFixedSize(28, 28);
    m_displayDetachButton->setIcon(
        m_displayDetachButton->style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_displayDetachButton->setToolTip("Open Display in a separate window");
    pageTools->addWidget(m_displayDetachButton);
    contentLayout->addLayout(pageTools);

    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addLayout(layout, 1);

    m_displayRestoreButton = new QToolButton(m_displayPage);
    m_displayRestoreButton->setObjectName("display_restore_button");
    m_displayRestoreButton->setAutoRaise(true);
    m_displayRestoreButton->setFixedSize(36, 36);
    m_displayRestoreButton->setIcon(
        m_displayRestoreButton->style()->standardIcon(QStyle::SP_TitleBarNormalButton));
    m_displayRestoreButton->setToolTip("Restore Display to the main window");
    m_displayRestoreButton->hide();
    hostLayout->addWidget(m_displayRestoreButton, 0, Qt::AlignCenter);

    m_displayFloatingWindow = new DisplayFloatingWindow([this] { attachDisplayPage(); }, this);
    m_displayFloatingWindow->setObjectName("display_floating_window");
    m_displayFloatingWindow->setWindowTitle("Display");
    m_displayFloatingWindow->resize(1200, 800);

    connect(m_displayDetachButton, &QToolButton::clicked, this, [this] {
        if (m_displayFloatingWindow != nullptr && m_displayFloatingWindow->isVisible()) {
            attachDisplayPage();
        } else {
            detachDisplayPage();
        }
    });
    connect(m_displayRestoreButton, &QToolButton::clicked, this, &MainWindow::attachDisplayPage);

    auto* display = &m_mainViewModel.display();

#ifdef DSS_HAS_OPENGL_WIDGETS
    const auto gpuDisplayDisabled =
        qEnvironmentVariableIntValue("DSS_DISABLE_GPU_IMAGE_DISPLAY") != 0;
    if (!gpuDisplayDisabled && GpuImageDisplay::isSupported()) {
        m_gpuImageDisplay = new GpuImageDisplay(m_displayPageContent);
        m_gpuImageDisplay->setDisplayStretch(display->displayAutoStretch(),
                                             display->displayStretchLow(),
                                             display->displayStretchHigh());
        m_imageDisplayWidget = m_gpuImageDisplay;
        display->setRawDisplayEnabled(true);
    }
#endif
    if (m_imageDisplayWidget == nullptr) {
        m_imageDisplay = new ImageDisplay(m_displayPageContent);
        m_imageDisplayWidget = m_imageDisplay;
        display->setRawDisplayEnabled(false);
    }
    m_imageDisplayWidget->setObjectName("main_image_display");
    layout->addWidget(m_imageDisplayWidget, 4);

    auto* stretchGroup = new QGroupBox("Display Stretch", m_displayPageContent);
    stretchGroup->setObjectName("display_stretch_group");
    auto* stretchForm = new QFormLayout(stretchGroup);
#ifdef DSS_HAS_ELA
    auto* autoStretch = new ElaCheckBox("Auto");
    auto* lowSlider = new ElaSlider(Qt::Horizontal);
    auto* highSlider = new ElaSlider(Qt::Horizontal);
#else
    auto* autoStretch = new QCheckBox("Auto");
    auto* lowSlider = new QSlider(Qt::Horizontal);
    auto* highSlider = new QSlider(Qt::Horizontal);
#endif
    auto* lowSpin = new WheelGuardedSpinBox;
    auto* highSpin = new WheelGuardedSpinBox;
    autoStretch->setObjectName("display_stretch_auto");
    lowSlider->setObjectName("display_stretch_low_slider");
    highSlider->setObjectName("display_stretch_high_slider");
    lowSpin->setObjectName("display_stretch_low_spin");
    highSpin->setObjectName("display_stretch_high_spin");

    lowSlider->setRange(0, 16383);
    highSlider->setRange(1, 16384);
    lowSpin->setRange(0, 16383);
    highSpin->setRange(1, 16384);
    lowSlider->setValue(display->displayStretchLow());
    highSlider->setValue(display->displayStretchHigh());
    lowSpin->setValue(display->displayStretchLow());
    highSpin->setValue(display->displayStretchHigh());
    autoStretch->setChecked(display->displayAutoStretch());

    auto* lowRow = new QHBoxLayout;
    lowRow->addWidget(lowSlider);
    lowRow->addWidget(lowSpin);
    auto* highRow = new QHBoxLayout;
    highRow->addWidget(highSlider);
    highRow->addWidget(highSpin);

    stretchForm->addRow("Auto", autoStretch);
    stretchForm->addRow("Low", lowRow);
    stretchForm->addRow("High", highRow);
    layout->addWidget(stretchGroup, 1);

    auto refreshStretchEnabled = [autoStretch, lowSlider, lowSpin, highSlider, highSpin] {
        const auto manualEnabled = !autoStretch->isChecked();
        lowSlider->setEnabled(manualEnabled);
        lowSpin->setEnabled(manualEnabled);
        highSlider->setEnabled(manualEnabled);
        highSpin->setEnabled(manualEnabled);
    };
    auto syncStretchControls = [lowSlider, lowSpin, highSlider, highSpin](int low, int high) {
        const QSignalBlocker lowSliderBlocker(lowSlider);
        const QSignalBlocker lowSpinBlocker(lowSpin);
        const QSignalBlocker highSliderBlocker(highSlider);
        const QSignalBlocker highSpinBlocker(highSpin);
        lowSlider->setValue(low);
        lowSpin->setValue(low);
        highSlider->setValue(high);
        highSpin->setValue(high);
    };
    enum class StretchEditedBound { Low, High };
    constexpr int kStretchPreviewIntervalMs = 40;
    auto* stretchThrottleTimer = new QTimer(m_displayPageContent);
    stretchThrottleTimer->setSingleShot(true);
    auto lastStretchApply = std::make_shared<QElapsedTimer>();
    auto lastEditedBound = std::make_shared<StretchEditedBound>(StretchEditedBound::Low);

    std::function<void()> applyStretchNow;
    applyStretchNow = [display, autoStretch, lowSpin, highSpin, syncStretchControls,
                       stretchThrottleTimer, lastStretchApply, lastEditedBound] {
        stretchThrottleTimer->stop();
        auto low = lowSpin->value();
        auto high = highSpin->value();
        if (low >= high) {
            if (*lastEditedBound == StretchEditedBound::High) {
                high = std::min(16384, low + 1);
            } else {
                low = std::max(0, high - 1);
            }
        }
        syncStretchControls(low, high);
        lastStretchApply->restart();
        (void)display->applyDisplayStretch(autoStretch->isChecked(), low, high);
    };
    const auto scheduleStretchPreview = [stretchThrottleTimer, lastStretchApply, applyStretchNow] {
        if (!lastStretchApply->isValid() ||
            lastStretchApply->elapsed() >= kStretchPreviewIntervalMs) {
            applyStretchNow();
            return;
        }
        if (!stretchThrottleTimer->isActive()) {
            const auto remainingMs = std::max(
                1, kStretchPreviewIntervalMs - static_cast<int>(lastStretchApply->elapsed()));
            stretchThrottleTimer->start(remainingMs);
        }
    };

    connect(stretchThrottleTimer, &QTimer::timeout, applyStretchNow);
    connect(autoStretch, &QCheckBox::toggled, [refreshStretchEnabled, applyStretchNow](bool) {
        refreshStretchEnabled();
        applyStretchNow();
    });
    connect(lowSlider, &QSlider::valueChanged,
            [lowSpin, lastEditedBound, scheduleStretchPreview](int value) {
                *lastEditedBound = StretchEditedBound::Low;
                const QSignalBlocker blocker(lowSpin);
                lowSpin->setValue(value);
                scheduleStretchPreview();
            });
    connect(lowSlider, &QSlider::sliderReleased, applyStretchNow);
    connect(lowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [lowSlider, lastEditedBound, scheduleStretchPreview](int value) {
                *lastEditedBound = StretchEditedBound::Low;
                const QSignalBlocker blocker(lowSlider);
                lowSlider->setValue(value);
                scheduleStretchPreview();
            });
    connect(highSlider, &QSlider::valueChanged,
            [highSpin, lastEditedBound, scheduleStretchPreview](int value) {
                *lastEditedBound = StretchEditedBound::High;
                const QSignalBlocker blocker(highSpin);
                highSpin->setValue(value);
                scheduleStretchPreview();
            });
    connect(highSlider, &QSlider::sliderReleased, applyStretchNow);
    connect(highSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [highSlider, lastEditedBound, scheduleStretchPreview](int value) {
                *lastEditedBound = StretchEditedBound::High;
                const QSignalBlocker blocker(highSlider);
                highSlider->setValue(value);
                scheduleStretchPreview();
            });
    connect(display, &DisplayViewModel::displayStretchChanged,
            [autoStretch, syncStretchControls, refreshStretchEnabled](bool autoScale, int low,
                                                                      int high) {
                const QSignalBlocker autoBlocker(autoStretch);
                autoStretch->setChecked(autoScale);
                syncStretchControls(low, high);
                refreshStretchEnabled();
            });
    refreshStretchEnabled();

    if (m_imageDisplay != nullptr) {
        connect(display, &DisplayViewModel::displayImageReady, m_imageDisplay,
                &ImageDisplay::setImage);
        connect(m_imageDisplay, &ImageDisplay::positionClicked, &AppEvent::instance(),
                &AppEvent::publishTargetPositionSelected);
    }
#ifdef DSS_HAS_OPENGL_WIDGETS
    if (m_gpuImageDisplay != nullptr) {
        connect(display, &DisplayViewModel::displayImageReady, m_gpuImageDisplay,
                &GpuImageDisplay::setImage);
        connect(display, &DisplayViewModel::rawDisplayFrameReady, m_gpuImageDisplay,
                &GpuImageDisplay::setRawFrame);
        connect(display, &DisplayViewModel::displayStretchChanged, m_gpuImageDisplay,
                &GpuImageDisplay::setDisplayStretch);
        connect(m_gpuImageDisplay, &GpuImageDisplay::positionClicked, &AppEvent::instance(),
                &AppEvent::publishTargetPositionSelected);
    }
#endif
}

void MainWindow::detachDisplayPage() {
    if (m_displayPage == nullptr || m_displayPageContent == nullptr ||
        m_displayFloatingWindow == nullptr || m_displayFloatingWindow->isVisible()) {
        return;
    }

    auto* hostLayout = qobject_cast<QVBoxLayout*>(m_displayPage->layout());
    if (hostLayout == nullptr) {
        return;
    }

    hostLayout->removeWidget(m_displayPageContent);
    m_displayFloatingWindow->setCentralWidget(m_displayPageContent);
    m_displayDetachButton->setIcon(
        m_displayDetachButton->style()->standardIcon(QStyle::SP_TitleBarNormalButton));
    m_displayDetachButton->setToolTip("Restore Display to the main window");
    m_displayRestoreButton->show();
    m_displayFloatingWindow->show();
    m_displayFloatingWindow->raise();
    m_displayFloatingWindow->activateWindow();
}

void MainWindow::attachDisplayPage() {
    if (m_displayPage == nullptr || m_displayPageContent == nullptr ||
        m_displayFloatingWindow == nullptr ||
        m_displayFloatingWindow->centralWidget() != m_displayPageContent) {
        return;
    }

    auto* hostLayout = qobject_cast<QVBoxLayout*>(m_displayPage->layout());
    if (hostLayout == nullptr) {
        return;
    }

    (void)m_displayFloatingWindow->takeCentralWidget();
    m_displayPageContent->setParent(m_displayPage);
    hostLayout->insertWidget(0, m_displayPageContent, 1);
    m_displayPageContent->show();
    m_displayDetachButton->setIcon(
        m_displayDetachButton->style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_displayDetachButton->setToolTip("Open Display in a separate window");
    m_displayRestoreButton->hide();
    m_displayFloatingWindow->hide();
}

}  // namespace Dss::Ui
