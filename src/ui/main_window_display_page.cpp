#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <algorithm>

#include "dss/ui/app_event.h"
#include "dss/ui/image_display.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaSlider.h>
#endif

namespace Dss::Ui {
void MainWindow::setupDisplayPage() {
    m_displayPage = new QWidget;
    auto* layout = new QHBoxLayout(m_displayPage);
    auto* display = &m_mainViewModel.display();

    m_imageDisplay = new ImageDisplay(m_displayPage);
    layout->addWidget(m_imageDisplay, 4);

    auto* stretchGroup = new QGroupBox("Display Stretch", m_displayPage);
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
    auto* lowSpin = new QSpinBox;
    auto* highSpin = new QSpinBox;
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
    auto applyStretch = [this, display, autoStretch, lowSpin, highSlider, highSpin,
                         syncStretchControls] {
        auto low = lowSpin->value();
        auto high = highSpin->value();
        if (low >= high) {
            if (this->sender() == highSlider || this->sender() == highSpin) {
                high = std::min(16384, low + 1);
            } else {
                low = std::max(0, high - 1);
            }
        }
        syncStretchControls(low, high);
        (void)display->applyDisplayStretch(autoStretch->isChecked(), low, high);
    };

    connect(autoStretch, &QCheckBox::toggled, [refreshStretchEnabled, applyStretch](bool) {
        refreshStretchEnabled();
        applyStretch();
    });
    connect(lowSlider, &QSlider::valueChanged, [lowSpin, applyStretch](int value) {
        const QSignalBlocker blocker(lowSpin);
        lowSpin->setValue(value);
        applyStretch();
    });
    connect(lowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [lowSlider, applyStretch](int value) {
                const QSignalBlocker blocker(lowSlider);
                lowSlider->setValue(value);
                applyStretch();
            });
    connect(highSlider, &QSlider::valueChanged, [highSpin, applyStretch](int value) {
        const QSignalBlocker blocker(highSpin);
        highSpin->setValue(value);
        applyStretch();
    });
    connect(highSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [highSlider, applyStretch](int value) {
                const QSignalBlocker blocker(highSlider);
                highSlider->setValue(value);
                applyStretch();
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

    connect(display, &DisplayViewModel::displayImageReady, m_imageDisplay, &ImageDisplay::setImage);
    connect(m_imageDisplay, &ImageDisplay::positionClicked, &AppEvent::instance(),
            &AppEvent::publishTargetPositionSelected);
}


}  // namespace Dss::Ui