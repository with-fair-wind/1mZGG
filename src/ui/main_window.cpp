#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "dss/ui/app_event.h"
#include "dss/ui/image_display.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaComboBox.h>
#include <ElaLog.h>
#include <ElaMessageBar.h>
#include <ElaPushButton.h>
#include <ElaSlider.h>
#include <ElaStatusBar.h>
#include <ElaTabWidget.h>
#include <ElaText.h>
#endif

namespace Dss::Ui {

#ifdef DSS_HAS_ELA
MainWindow::MainWindow(ViewModel& vm, QWidget* parent) : ElaWindow(parent), m_vm(vm) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    connectSignals();
}
#else
MainWindow::MainWindow(ViewModel& vm, QWidget* parent) : QMainWindow(parent), m_vm(vm) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    connectSignals();
}
#endif

MainWindow::~MainWindow() = default;

/// 创建各功能页并注册到导航（Ela 侧栏或 QTabWidget）
void MainWindow::setupNavigation() {
    setupControlPage();
    setupDisplayPage();
    setupAnalysisPage();
    setupCommStatusPage();
    setupSettingsPage();
    setupLogPage();

#ifdef DSS_HAS_ELA
    addPageNode("Control", m_controlPage, ElaIconType::Gamepad);
    addPageNode("Display", m_displayPage, ElaIconType::Display);
    addPageNode("Analysis", m_analysisPage, ElaIconType::ChartLine);
    addPageNode("Communication", m_commPage, ElaIconType::Satellite);
    addPageNode("Settings", m_settingsPage, ElaIconType::Gear);
    addPageNode("Logs", m_logPage, ElaIconType::FileLines);
#else
    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_controlPage, "Control");
    tabs->addTab(m_displayPage, "Display");
    tabs->addTab(m_analysisPage, "Analysis");
    tabs->addTab(m_commPage, "Communication");
    tabs->addTab(m_settingsPage, "Settings");
    tabs->addTab(m_logPage, "Logs");
    setCentralWidget(tabs);
#endif
}

/// 构建序列选择、回放控制、处理/跟踪模式及统计信息显示
void MainWindow::setupControlPage() {
    m_controlPage = new QWidget;
    auto* layout = new QVBoxLayout(m_controlPage);

    auto* sequenceRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnSelectSequence = new ElaPushButton("Select Sequence");
#else
    auto* btnSelectSequence = new QPushButton("Select Sequence");
#endif
    auto* sequenceLabel = new QLabel("Frames: 0");
    auto* currentFrameLabel = new QLabel("Current: 0/0");
    sequenceRow->addWidget(btnSelectSequence);
    sequenceRow->addWidget(sequenceLabel);
    sequenceRow->addWidget(currentFrameLabel);

    connect(btnSelectSequence, &QPushButton::clicked, [this] {
        const auto files = QFileDialog::getOpenFileNames(
            this, "Select Image Sequence", QString(),
            "Image Sequence (*.bmp *.png *.jpg *.jpeg *.tif *.tiff *.raw);;All Files (*)");
        if (!files.isEmpty()) {
            m_vm.selectReplayFiles(files);
        }
    });
    connect(&m_vm, &ViewModel::replayFrameCountChanged, [sequenceLabel](int count) {
        sequenceLabel->setText(QString("Frames: %1").arg(count));
    });
    connect(&m_vm, &ViewModel::replayFrameCountChanged, [this, currentFrameLabel](int count) {
        currentFrameLabel->setText(
            QString("Current: %1/%2").arg(m_vm.replayCurrentFrame()).arg(count));
    });
    connect(&m_vm, &ViewModel::replayCurrentFrameChanged, [this, currentFrameLabel](int frame) {
        currentFrameLabel->setText(
            QString("Current: %1/%2").arg(frame).arg(m_vm.replayFrameCount()));
    });

    auto* grabRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnStart = new ElaPushButton("Start Replay");
    auto* btnStop = new ElaPushButton("Pause Replay");
    auto* btnStepForward = new ElaPushButton("Step Forward");
#else
    auto* btnStart = new QPushButton("Start Replay");
    auto* btnStop = new QPushButton("Pause Replay");
    auto* btnStepForward = new QPushButton("Step Forward");
#endif
    grabRow->addWidget(btnStart);
    grabRow->addWidget(btnStop);
    grabRow->addWidget(btnStepForward);

    connect(btnStart, &QPushButton::clicked, &m_vm, &ViewModel::startGrab);
    connect(btnStop, &QPushButton::clicked, &m_vm, &ViewModel::stopGrab);
    connect(btnStepForward, &QPushButton::clicked, &m_vm, &ViewModel::stepReplayForward);

    auto* processingRow = new QHBoxLayout;
    processingRow->addWidget(new QLabel("Processing:"));
#ifdef DSS_HAS_ELA
    auto* processingCombo = new ElaComboBox;
#else
    auto* processingCombo = new QComboBox;
#endif
    processingCombo->addItems({"None", "OpenCV"});
    processingRow->addWidget(processingCombo);

    connect(processingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                static constexpr int modeMap[] = {0, 3};
                if (index >= 0 && index < 2) {
                    m_vm.setProcessingMode(modeMap[index]);
                }
            });

    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("Track Mode:"));
#ifdef DSS_HAS_ELA
    auto* modeCombo = new ElaComboBox;
#else
    auto* modeCombo = new QComboBox;
#endif
    modeCombo->addItems({"Init", "GEO", "SC", "LEO", "Manual"});
    modeRow->addWidget(modeCombo);

    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        static constexpr int modeMap[] = {-1, 0, 3, 4, 5};
        if (index >= 0 && index < 5) {
            m_vm.setTrackMode(modeMap[index]);
        }
    });

    auto* expRow = new QHBoxLayout;
    expRow->addWidget(new QLabel("Exposure (ms):"));
#ifdef DSS_HAS_ELA
    auto* expSlider = new ElaSlider(Qt::Horizontal);
#else
    auto* expSlider = new QSlider(Qt::Horizontal);
#endif
    expSlider->setRange(1, 1000);
    expSlider->setValue(100);
    auto* expLabel = new QLabel("100");
    expRow->addWidget(expSlider);
    expRow->addWidget(expLabel);

    connect(expSlider, &QSlider::valueChanged, [this, expLabel](int val) {
        expLabel->setText(QString::number(val));
        m_vm.setExposure(val);
    });

    auto* saveRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* chkSave = new ElaCheckBox("Save Data");
#else
    auto* chkSave = new QCheckBox("Save Data");
#endif
    saveRow->addWidget(chkSave);
    connect(chkSave, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_vm.startSaving();
        } else {
            m_vm.stopSaving();
        }
    });

    layout->addLayout(sequenceRow);
    layout->addLayout(grabRow);
    layout->addLayout(processingRow);
    layout->addLayout(modeRow);
    layout->addLayout(expRow);
    layout->addLayout(saveRow);

    auto* statsLabel = new QLabel("Image: -- | FPS: --");
    statsLabel->setObjectName("stats_label");
    layout->addWidget(statsLabel);

    connect(&m_vm, &ViewModel::imageStatsUpdated,
            [statsLabel](double minVal, double maxVal, double avg, double stdDev) {
                statsLabel->setText(QString("Min: %1 | Max: %2 | Avg: %3 | Std: %4")
                                        .arg(minVal, 0, 'f', 1)
                                        .arg(maxVal, 0, 'f', 1)
                                        .arg(avg, 0, 'f', 1)
                                        .arg(stdDev, 0, 'f', 1));
            });

    layout->addStretch();
}

/// 放置主图像显示控件，并绑定 ViewModel 与 AppEvent
void MainWindow::setupDisplayPage() {
    m_displayPage = new QWidget;
    auto* layout = new QHBoxLayout(m_displayPage);

    m_imageDisplay = new ImageDisplay(m_displayPage);
    layout->addWidget(m_imageDisplay, 3);

    connect(&m_vm, &ViewModel::displayImageReady, m_imageDisplay, &ImageDisplay::setImage);
    connect(m_imageDisplay, &ImageDisplay::positionClicked, &AppEvent::instance(),
            &AppEvent::publishTargetPositionSelected);
}

void MainWindow::setupAnalysisPage() {
    m_analysisPage = new QWidget;
    auto* layout = new QVBoxLayout(m_analysisPage);
    layout->addWidget(new QLabel("Analysis / Curve Charts"));
    // TODO: integrate ElaCharts or QChartView for distance curves
    layout->addStretch();
}

void MainWindow::setupCommStatusPage() {
    m_commPage = new QWidget;
    auto* layout = new QVBoxLayout(m_commPage);
    layout->addWidget(new QLabel("Communication Status Monitor"));
    // TODO: add serial/network status indicators
    layout->addStretch();
}

void MainWindow::setupSettingsPage() {
    m_settingsPage = new QWidget;
    auto* layout = new QVBoxLayout(m_settingsPage);
    layout->addWidget(new QLabel("System Settings"));
    // TODO: settings form for optics, tracking, paths
    layout->addStretch();
}

void MainWindow::setupLogPage() {
    m_logPage = new QWidget;
    auto* layout = new QVBoxLayout(m_logPage);
#ifdef DSS_HAS_ELA
    auto* logWidget = new ElaLog;
    layout->addWidget(logWidget);
#else
    auto* logText = new QTextEdit;
    logText->setReadOnly(true);
    layout->addWidget(logText);
#endif
}

/// 连接 AppEvent 与 ViewModel，并在状态栏/MessageBar 显示反馈
void MainWindow::connectSignals() {
    connect(&AppEvent::instance(), &AppEvent::targetPositionSelected, &m_vm,
            &ViewModel::selectTarget);
    connect(&AppEvent::instance(), &AppEvent::zoomLevelChanged, &m_vm, &ViewModel::toggleZoom);

    connect(&m_vm, &ViewModel::statusTextChanged, [this](const QString& text) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::success(ElaMessageBarType::TopRight, "Status", text, 2000, this);
#else
        statusBar()->showMessage(text, 3000);
#endif
    });

    connect(&m_vm, &ViewModel::trackInfoUpdated, [this](const QString& info) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "Track", info, 3000, this);
#else
        statusBar()->showMessage(info, 5000);
#endif
    });
}

}  // namespace Dss::Ui
