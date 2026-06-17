#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>
#include <vector>

#include "dss/ui/app_event.h"
#include "dss/ui/image_display.h"
#include "dss/ui/log_palette.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaComboBox.h>
#include <ElaMessageBar.h>
#include <ElaPushButton.h>
#include <ElaSlider.h>
#include <ElaStatusBar.h>
#include <ElaTabWidget.h>
#include <ElaText.h>
#endif

namespace Dss::Ui {

namespace {

/// @brief 将日志快照按级别颜色渲染到文本控件。
void renderColoredLogEntries(QTextEdit& logText, const QStringList& entries) {
    logText.clear();

    QTextCursor cursor(logText.document());
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        const auto level = inferLogLevelFromText(entry);

        QTextCharFormat format;
        format.setForeground(logTextColor(level));
        if (level == Dss::Core::LogLevel::Error) {
            format.setFontWeight(QFont::DemiBold);
        }

        cursor.insertText(entry, format);
        if (index + 1 < entries.size()) {
            cursor.insertBlock();
        }
    }
    logText.moveCursor(QTextCursor::End);
}

/// @brief 为内容较高的页面创建可滚动容器，避免页面最小高度撑大主窗口。
/// @param content 页面内容。
/// @return 可直接注册到主导航的滚动区域。
auto makeScrollablePage(QWidget* content) -> QScrollArea* {
    auto* scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(content);
    return scrollArea;
}

}  // namespace

#ifdef DSS_HAS_ELA
MainWindow::MainWindow(MainViewModel& mainViewModel, QWidget* parent)
    : ElaWindow(parent), m_mainViewModel(mainViewModel) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    connectSignals();
}
#else
MainWindow::MainWindow(MainViewModel& mainViewModel, QWidget* parent)
    : QMainWindow(parent), m_mainViewModel(mainViewModel) {
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
    addPageNode("Communication", makeScrollablePage(m_commPage), ElaIconType::Satellite);
    addPageNode("Settings", m_settingsPage, ElaIconType::Gear);
    addPageNode("Logs", m_logPage, ElaIconType::FileLines);
#else
    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_controlPage, "Control");
    tabs->addTab(m_displayPage, "Display");
    tabs->addTab(m_analysisPage, "Analysis");
    tabs->addTab(makeScrollablePage(m_commPage), "Communication");
    tabs->addTab(m_settingsPage, "Settings");
    tabs->addTab(m_logPage, "Logs");
    setCentralWidget(tabs);
#endif
}

/// 构建序列选择、回放控制、处理/跟踪模式及统计信息显示
void MainWindow::setupControlPage() {
    m_controlPage = new QWidget;
    auto* layout = new QVBoxLayout(m_controlPage);
    auto* replay = &m_mainViewModel.replay();
    auto* display = &m_mainViewModel.display();
    auto* processing = &m_mainViewModel.processing();
    auto* tracking = &m_mainViewModel.tracking();
    auto* storage = &m_mainViewModel.storage();

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

    connect(btnSelectSequence, &QPushButton::clicked, [this, replay] {
        const auto files = QFileDialog::getOpenFileNames(
            this, "Select Image Sequence", QString(),
            "Image Sequence (*.bmp *.png *.jpg *.jpeg *.tif *.tiff *.raw);;All Files (*)");
        if (!files.isEmpty()) {
            replay->selectReplayFiles(files);
        }
    });
    connect(replay, &ReplayViewModel::replayFrameCountChanged, [sequenceLabel](int count) {
        sequenceLabel->setText(QString("Frames: %1").arg(count));
    });
    connect(replay, &ReplayViewModel::replayFrameCountChanged,
            [replay, currentFrameLabel](int count) {
                currentFrameLabel->setText(
                    QString("Current: %1/%2").arg(replay->replayCurrentFrame()).arg(count));
            });
    connect(replay, &ReplayViewModel::replayCurrentFrameChanged,
            [replay, currentFrameLabel](int frame) {
                currentFrameLabel->setText(
                    QString("Current: %1/%2").arg(frame).arg(replay->replayFrameCount()));
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

    connect(btnStart, &QPushButton::clicked, replay, &ReplayViewModel::startGrab);
    connect(btnStop, &QPushButton::clicked, replay, &ReplayViewModel::stopGrab);
    connect(btnStepForward, &QPushButton::clicked, replay, &ReplayViewModel::stepReplayForward);

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
            [processing](int index) {
                static constexpr int modeMap[] = {0, 3};
                if (index >= 0 && index < 2) {
                    processing->setProcessingMode(modeMap[index]);
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

    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [tracking](int index) {
        static constexpr int modeMap[] = {-1, 0, 3, 4, 5};
        if (index >= 0 && index < 5) {
            tracking->setTrackMode(modeMap[index]);
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
        m_mainViewModel.setExposure(val);
    });

    auto* saveRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* chkSave = new ElaCheckBox("Save Data");
#else
    auto* chkSave = new QCheckBox("Save Data");
#endif
    saveRow->addWidget(chkSave);
    connect(chkSave, &QCheckBox::toggled, [storage](bool checked) {
        if (checked) {
            storage->startSaving();
        } else {
            storage->stopSaving();
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

    connect(display, &DisplayViewModel::imageStatsUpdated,
            [statsLabel](double minVal, double maxVal, double avg, double stdDev) {
                statsLabel->setText(QString("Min: %1 | Max: %2 | Avg: %3 | Std: %4")
                                        .arg(minVal, 0, 'f', 1)
                                        .arg(maxVal, 0, 'f', 1)
                                        .arg(avg, 0, 'f', 1)
                                        .arg(stdDev, 0, 'f', 1));
            });

    layout->addStretch();
}

/// 放置主图像显示控件，并绑定显示子 ViewModel 与 AppEvent
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
    auto* serial = &m_mainViewModel.serialPorts();
    auto* network = &m_mainViewModel.network();
    auto* dataExchange = &m_mainViewModel.dataExchange();
    layout->addWidget(new QLabel("Communication Status Monitor"));

    /// @brief 通信页中单个串口通道的控件集合。
    struct SerialEditor {
        QString key;               ///< 串口通道键名
        QLineEdit* portName;       ///< 端口名称输入框
        QSpinBox* baudRate;        ///< 波特率输入框
        QSpinBox* dataBits;        ///< 数据位输入框
        QSpinBox* stopBits;        ///< 停止位输入框
        QLabel* frameSize;         ///< 收发帧长标签
        QLabel* serviceState;      ///< 服务打开状态标签
        QPushButton* applyButton;  ///< 应用配置按钮
        QPushButton* openButton;   ///< 显式打开串口按钮
        QPushButton* closeButton;  ///< 显式关闭串口按钮
    };

    struct EndpointEditor {
        QString key;               ///< 端点键名
        QLineEdit* localIp;        ///< 本地 IP 输入框
        QSpinBox* localPort;       ///< 本地端口输入框
        QLineEdit* remoteIp;       ///< 远端 IP 输入框
        QSpinBox* remotePort;      ///< 远端端口输入框
        QLabel* serviceState;      ///< 服务打开状态标签
        QPushButton* openButton;   ///< 显式打开服务按钮
        QPushButton* closeButton;  ///< 显式关闭服务按钮
        bool canOpen = false;      ///< 是否支持显式打开/关闭
    };
    auto serialEditors = std::make_shared<std::vector<SerialEditor>>();
    auto endpointEditors = std::make_shared<std::vector<EndpointEditor>>();

    auto makeLineEdit = [](const QString& text) {
        auto* edit = new QLineEdit(text);
        edit->setClearButtonEnabled(true);
        return edit;
    };
    auto makePortSpin = [](int minimum, int value) {
        auto* spin = new QSpinBox;
        spin->setRange(minimum, 65535);
        spin->setValue(value);
        return spin;
    };
    auto makeSerialSpin = [](int minimum, int maximum, int value) {
        auto* spin = new QSpinBox;
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        return spin;
    };
    auto makeDoubleSpin = [](double minimum, double maximum, double value) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(minimum, maximum);
        spin->setDecimals(3);
        spin->setValue(value);
        return spin;
    };

    auto makeSerialGroup = [serial, serialEditors, makeLineEdit,
                            makeSerialSpin](const SerialChannelState& channel) {
        auto* group = new QGroupBox(channel.displayName);
        auto* form = new QFormLayout(group);
        auto* portName = makeLineEdit(channel.portName);
        auto* baudRate = makeSerialSpin(1, 4'000'000, channel.baudRate);
        auto* dataBits = makeSerialSpin(5, 8, channel.dataBits);
        auto* stopBits = makeSerialSpin(1, 2, channel.stopBits);
        auto* frameSize = new QLabel;
        auto* serviceState = new QLabel;
#ifdef DSS_HAS_ELA
        auto* btnApplySerial = new ElaPushButton("Apply");
        auto* btnOpenSerial = new ElaPushButton("Open");
        auto* btnCloseSerial = new ElaPushButton("Close");
#else
        auto* btnApplySerial = new QPushButton("Apply");
        auto* btnOpenSerial = new QPushButton("Open");
        auto* btnCloseSerial = new QPushButton("Close");
#endif
        auto* serviceRow = new QHBoxLayout;
        serviceRow->addWidget(btnOpenSerial);
        serviceRow->addWidget(btnCloseSerial);
        serviceRow->addWidget(serviceState);
        serviceRow->addStretch();

        serialEditors->push_back(SerialEditor{channel.key, portName, baudRate, dataBits, stopBits,
                                              frameSize, serviceState, btnApplySerial,
                                              btnOpenSerial, btnCloseSerial});

        form->addRow("Port", portName);
        form->addRow("Baud", baudRate);
        form->addRow("Data Bits", dataBits);
        form->addRow("Stop Bits", stopBits);
        form->addRow("Frames", frameSize);
        form->addRow(btnApplySerial);
        form->addRow("Service", serviceRow);

        connect(btnApplySerial, &QPushButton::clicked, [serial, serialEditors, key = channel.key] {
            for (const auto& editor : *serialEditors) {
                if (editor.key == key) {
                    serial->applySerialChannelConfig(
                        editor.key, editor.portName->text(), editor.baudRate->value(),
                        editor.dataBits->value(), editor.stopBits->value());
                    return;
                }
            }
        });
        connect(btnOpenSerial, &QPushButton::clicked,
                [serial, key = channel.key] { serial->openSerialChannel(key); });
        connect(btnCloseSerial, &QPushButton::clicked,
                [serial, key = channel.key] { serial->closeSerialChannel(key); });
        return group;
    };

    auto makeEndpointGroup = [network, endpointEditors, makeLineEdit,
                              makePortSpin](const NetworkEndpointState& endpoint) {
        auto* group = new QGroupBox(endpoint.displayName);
        auto* form = new QFormLayout(group);
        auto* localIp = makeLineEdit(endpoint.localIp);
        auto* localPort = makePortSpin(0, endpoint.localPort);
        auto* remoteIp = makeLineEdit(endpoint.remoteIp);
        auto* remotePort = makePortSpin(0, endpoint.remotePort);

#ifdef DSS_HAS_ELA
        auto* btnApplyEndpoint = new ElaPushButton("Apply");
#else
        auto* btnApplyEndpoint = new QPushButton("Apply");
#endif
        auto* serviceState = new QLabel;
        QPushButton* btnOpenEndpoint = nullptr;
        QPushButton* btnCloseEndpoint = nullptr;
        auto* serviceRow = new QHBoxLayout;
        if (endpoint.canOpen) {
#ifdef DSS_HAS_ELA
            btnOpenEndpoint = new ElaPushButton("Open");
            btnCloseEndpoint = new ElaPushButton("Close");
#else
            btnOpenEndpoint = new QPushButton("Open");
            btnCloseEndpoint = new QPushButton("Close");
#endif
            serviceRow->addWidget(btnOpenEndpoint);
            serviceRow->addWidget(btnCloseEndpoint);
        }
        serviceRow->addWidget(serviceState);
        serviceRow->addStretch();
        endpointEditors->push_back(EndpointEditor{endpoint.key, localIp, localPort, remoteIp,
                                                  remotePort, serviceState, btnOpenEndpoint,
                                                  btnCloseEndpoint, endpoint.canOpen});

        form->addRow("Local IP", localIp);
        form->addRow("Local Port", localPort);
        form->addRow("Remote IP", remoteIp);
        form->addRow("Remote Port", remotePort);
        form->addRow(btnApplyEndpoint);
        form->addRow("Service", serviceRow);

        connect(btnApplyEndpoint, &QPushButton::clicked,
                [network, endpointEditors, key = endpoint.key] {
                    for (const auto& editor : *endpointEditors) {
                        if (editor.key == key) {
                            network->applyNetworkEndpointConfig(
                                editor.key, editor.localIp->text(), editor.localPort->value(),
                                editor.remoteIp->text(), editor.remotePort->value());
                            return;
                        }
                    }
                });
        if (endpoint.canOpen) {
            connect(btnOpenEndpoint, &QPushButton::clicked,
                    [network, key = endpoint.key] { network->openNetworkEndpoint(key); });
            connect(btnCloseEndpoint, &QPushButton::clicked,
                    [network, key = endpoint.key] { network->closeNetworkEndpoint(key); });
        }
        return group;
    };

    auto makeExposureCommandGroup = [serial, makeSerialSpin] {
        auto* group = new QGroupBox("Exposure Command");
        auto* form = new QFormLayout(group);
#ifdef DSS_HAS_ELA
        auto* freeRun = new ElaCheckBox("Free Run");
        auto* sendButton = new ElaPushButton("Send");
#else
        auto* freeRun = new QCheckBox("Free Run");
        auto* sendButton = new QPushButton("Send");
#endif
        auto* frameFrequency = makeSerialSpin(0, 255, 1);
        auto* delayTicks = makeSerialSpin(0, 0xFF'FFFF, 0);

        form->addRow("Trigger", freeRun);
        form->addRow("Frame Code", frameFrequency);
        form->addRow("Delay Ticks", delayTicks);
        form->addRow(sendButton);

        connect(sendButton, &QPushButton::clicked, [serial, freeRun, frameFrequency, delayTicks] {
            serial->sendExposureCommand(freeRun->isChecked(), frameFrequency->value(),
                                        delayTicks->value());
        });
        return group;
    };

    auto makeServoCommandGroup = [serial, makeSerialSpin, makeDoubleSpin] {
        auto* group = new QGroupBox("Servo Correction");
        auto* form = new QFormLayout(group);
#ifdef DSS_HAS_ELA
        auto* distanceValid = new ElaCheckBox("Distance");
        auto* speedValid = new ElaCheckBox("Speed");
        auto* sendButton = new ElaPushButton("Send");
#else
        auto* distanceValid = new QCheckBox("Distance");
        auto* speedValid = new QCheckBox("Speed");
        auto* sendButton = new QPushButton("Send");
#endif
        auto* distanceX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* distanceY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* speedX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* speedY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* mode = makeSerialSpin(0, 255, 0x19);

        form->addRow("Distance Valid", distanceValid);
        form->addRow("Distance X", distanceX);
        form->addRow("Distance Y", distanceY);
        form->addRow("Speed Valid", speedValid);
        form->addRow("Speed X", speedX);
        form->addRow("Speed Y", speedY);
        form->addRow("Mode", mode);
        form->addRow(sendButton);

        connect(sendButton, &QPushButton::clicked,
                [serial, distanceValid, speedValid, distanceX, distanceY, speedX, speedY, mode] {
                    serial->sendServoCorrection(distanceValid->isChecked(), speedValid->isChecked(),
                                                distanceX->value(), distanceY->value(),
                                                speedX->value(), speedY->value(), mode->value());
                });
        return group;
    };

    auto makeMasterControlCommandGroup = [serial, makeSerialSpin, makeDoubleSpin] {
        auto* group = new QGroupBox("Master Control Status");
        auto* form = new QFormLayout(group);
#ifdef DSS_HAS_ELA
        auto* distanceValid = new ElaCheckBox("Distance");
        auto* speedValid = new ElaCheckBox("Speed");
        auto* sendButton = new ElaPushButton("Send");
#else
        auto* distanceValid = new QCheckBox("Distance");
        auto* speedValid = new QCheckBox("Speed");
        auto* sendButton = new QPushButton("Send");
#endif
        auto* azimuth = makeDoubleSpin(0.0, 360.0, 0.0);
        auto* elevation = makeDoubleSpin(0.0, 360.0, 0.0);
        auto* distanceX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* distanceY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* speedX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* speedY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, 0.0);
        auto* mode = makeSerialSpin(0, 255, 0x19);

        form->addRow("Azimuth", azimuth);
        form->addRow("Elevation", elevation);
        form->addRow("Distance Valid", distanceValid);
        form->addRow("Distance X", distanceX);
        form->addRow("Distance Y", distanceY);
        form->addRow("Speed Valid", speedValid);
        form->addRow("Speed X", speedX);
        form->addRow("Speed Y", speedY);
        form->addRow("Mode", mode);
        form->addRow(sendButton);

        connect(sendButton, &QPushButton::clicked,
                [serial, azimuth, elevation, distanceValid, speedValid, distanceX, distanceY,
                 speedX, speedY, mode] {
                    const auto now = QDateTime::currentDateTime();
                    const auto date = now.date();
                    const auto time = now.time();
                    serial->sendMasterControlStatus(
                        date.year(), date.month(), date.day(), time.hour(), time.minute(),
                        time.second(), time.msec(), azimuth->value(), elevation->value(),
                        distanceValid->isChecked(), speedValid->isChecked(), distanceX->value(),
                        distanceY->value(), speedX->value(), speedY->value(), mode->value());
                });
        return group;
    };

    layout->addWidget(new QLabel("Serial Channels"));
    auto* serialGrid = new QGridLayout;
    const auto serialChannels = serial->serialChannelConfigs();
    for (int index = 0; index < static_cast<int>(serialChannels.size()); ++index) {
        serialGrid->addWidget(makeSerialGroup(serialChannels[static_cast<std::size_t>(index)]),
                              index / 2, index % 2);
    }
    layout->addLayout(serialGrid);

    layout->addWidget(new QLabel("Serial Commands"));
    auto* serialCommandGrid = new QGridLayout;
    serialCommandGrid->addWidget(makeExposureCommandGroup(), 0, 0);
    serialCommandGrid->addWidget(makeServoCommandGroup(), 0, 1);
    serialCommandGrid->addWidget(makeMasterControlCommandGroup(), 1, 0, 1, 2);
    layout->addLayout(serialCommandGrid);

    layout->addWidget(new QLabel("UDP Endpoints"));
    auto* endpointGrid = new QGridLayout;
    const auto endpoints = network->networkEndpointConfigs();
    for (int index = 0; index < static_cast<int>(endpoints.size()); ++index) {
        endpointGrid->addWidget(makeEndpointGroup(endpoints[static_cast<std::size_t>(index)]),
                                index / 2, index % 2);
    }
    layout->addLayout(endpointGrid);

    auto* actionRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnOpenDataExchange = new ElaPushButton("Open UDP");
    auto* btnCloseDataExchange = new ElaPushButton("Close UDP");
#else
    auto* btnOpenDataExchange = new QPushButton("Open UDP");
    auto* btnCloseDataExchange = new QPushButton("Close UDP");
#endif
    auto* dataExchangeState = new QLabel;
    actionRow->addWidget(btnOpenDataExchange);
    actionRow->addWidget(btnCloseDataExchange);
    actionRow->addWidget(dataExchangeState);
    actionRow->addStretch();
    layout->addLayout(actionRow);

    auto refreshEndpoints = [network, endpointEditors] {
        const auto currentEndpoints = network->networkEndpointConfigs();
        for (const auto& endpoint : currentEndpoints) {
            for (const auto& editor : *endpointEditors) {
                if (editor.key == endpoint.key) {
                    editor.localIp->setText(endpoint.localIp);
                    editor.localPort->setValue(endpoint.localPort);
                    editor.remoteIp->setText(endpoint.remoteIp);
                    editor.remotePort->setValue(endpoint.remotePort);
                    editor.serviceState->setText(
                        endpoint.canOpen ? (endpoint.isOpen ? "Service: opened" : "Service: closed")
                                         : "Service: use Data Exchange controls");
                    if (editor.openButton != nullptr) {
                        editor.openButton->setEnabled(endpoint.canOpen && !endpoint.isOpen);
                    }
                    if (editor.closeButton != nullptr) {
                        editor.closeButton->setEnabled(endpoint.canOpen && endpoint.isOpen);
                    }
                }
            }
        }
    };
    auto refreshSerialChannels = [serial, serialEditors] {
        const auto currentChannels = serial->serialChannelConfigs();
        for (const auto& channel : currentChannels) {
            for (const auto& editor : *serialEditors) {
                if (editor.key == channel.key) {
                    editor.portName->setText(channel.portName);
                    editor.baudRate->setValue(channel.baudRate);
                    editor.dataBits->setValue(channel.dataBits);
                    editor.stopBits->setValue(channel.stopBits);
                    editor.frameSize->setText(QString("rx %1 / tx %2")
                                                  .arg(channel.recvFrameSize)
                                                  .arg(channel.sendFrameSize));
                    editor.serviceState->setText(
                        channel.isRegistered
                            ? (channel.isOpen ? "Service: opened" : "Service: closed")
                            : "Service: not registered");
                    editor.applyButton->setEnabled(true);
                    editor.openButton->setEnabled(channel.isRegistered && !channel.isOpen);
                    editor.closeButton->setEnabled(channel.isRegistered && channel.isOpen);
                }
            }
        }
    };
    auto refreshOpenState = [dataExchange, btnOpenDataExchange, btnCloseDataExchange,
                             dataExchangeState] {
        const auto isOpen = dataExchange->isDataExchangeOpen();
        btnOpenDataExchange->setEnabled(!isOpen);
        btnCloseDataExchange->setEnabled(isOpen);
        dataExchangeState->setText(isOpen ? "Data exchange: opened" : "Data exchange: closed");
    };

    connect(btnOpenDataExchange, &QPushButton::clicked, dataExchange,
            &DataExchangeViewModel::openDataExchange);
    connect(btnCloseDataExchange, &QPushButton::clicked, dataExchange,
            &DataExchangeViewModel::closeDataExchange);
    connect(serial, &SerialPortViewModel::serialChannelsChanged, refreshSerialChannels);
    connect(serial, &SerialPortViewModel::serialChannelStateChanged,
            [refreshSerialChannels](const QString&, bool) { refreshSerialChannels(); });
    connect(network, &NetworkViewModel::networkEndpointsChanged, refreshEndpoints);
    connect(network, &NetworkViewModel::dataExchangeEndpointsChanged, refreshEndpoints);
    connect(dataExchange, &DataExchangeViewModel::dataExchangeEndpointsChanged, refreshEndpoints);
    connect(dataExchange, &DataExchangeViewModel::dataExchangeOpenChanged, refreshOpenState);

    refreshSerialChannels();
    refreshEndpoints();
    refreshOpenState();
    layout->addStretch();
}

void MainWindow::setupSettingsPage() {
    m_settingsPage = new QWidget;
    auto* layout = new QVBoxLayout(m_settingsPage);
    layout->addWidget(new QLabel("System Settings"));
    // TODO: settings form for optics, tracking, paths
    layout->addStretch();
}

/// @brief 创建日志页并连接日志子 ViewModel 事件。
void MainWindow::setupLogPage() {
    m_logPage = new QWidget;
    auto* layout = new QVBoxLayout(m_logPage);
    auto& logs = m_mainViewModel.logs();

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel("Level:"));
#ifdef DSS_HAS_ELA
    auto* levelCombo = new ElaComboBox;
#else
    auto* levelCombo = new QComboBox;
#endif
    levelCombo->addItems({"Info", "Warning", "Error"});
    levelCombo->setCurrentIndex(logs.logMinimumLevel());
    filterRow->addWidget(levelCombo);
    filterRow->addStretch();
    layout->addLayout(filterRow);

    auto* logText = new QTextEdit;
    logText->setReadOnly(true);
    logText->setLineWrapMode(QTextEdit::NoWrap);
    auto refreshLogText = [logText](const QStringList& entries) {
        renderColoredLogEntries(*logText, entries);
    };
    connect(&logs, &LogViewModel::logEntriesChanged, logText, refreshLogText);
    connect(levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &logs,
            &LogViewModel::setLogMinimumLevel);
    refreshLogText(logs.visibleLogEntries());
    layout->addWidget(logText);
}

/// 连接 AppEvent 与 UI 层主 ViewModel、子 ViewModel，并在状态栏/MessageBar 显示反馈
void MainWindow::connectSignals() {
    auto* display = &m_mainViewModel.display();
    auto* tracking = &m_mainViewModel.tracking();

    connect(&AppEvent::instance(), &AppEvent::targetPositionSelected, tracking,
            &TrackingViewModel::selectTarget);
    connect(&AppEvent::instance(), &AppEvent::zoomLevelChanged, display,
            &DisplayViewModel::toggleZoom);

    connect(&m_mainViewModel, &MainViewModel::statusTextChanged, [this](const QString& text) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::success(ElaMessageBarType::TopRight, "Status", text, 2000, this);
#else
        statusBar()->showMessage(text, 3000);
#endif
    });

    connect(tracking, &TrackingViewModel::trackInfoUpdated, [this](const QString& info) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "Track", info, 3000, this);
#else
        statusBar()->showMessage(info, 5000);
#endif
    });
}

}  // namespace Dss::Ui
