#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <memory>
#include <vector>

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

    /// @brief 通信页中单个串口通道的控件集合。
    struct SerialEditor {
        QString key;               ///< 串口通道键名
        QLabel* portName;          ///< 端口名称标签
        QLabel* baudRate;          ///< 波特率标签
        QLabel* format;            ///< 数据位/停止位标签
        QLabel* frameSize;         ///< 收发帧长标签
        QLabel* serviceState;      ///< 服务打开状态标签
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

    auto makeIpEdit = [](const QString& text) {
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

    auto makeSerialGroup = [this, serialEditors](const SerialChannelState& channel) {
        auto* group = new QGroupBox(channel.displayName);
        auto* form = new QFormLayout(group);
        auto* portName = new QLabel(channel.portName);
        auto* baudRate = new QLabel(QString::number(channel.baudRate));
        auto* format =
            new QLabel(QString("%1 data / %2 stop").arg(channel.dataBits).arg(channel.stopBits));
        auto* frameSize = new QLabel;
        auto* serviceState = new QLabel;
#ifdef DSS_HAS_ELA
        auto* btnOpenSerial = new ElaPushButton("Open");
        auto* btnCloseSerial = new ElaPushButton("Close");
#else
        auto* btnOpenSerial = new QPushButton("Open");
        auto* btnCloseSerial = new QPushButton("Close");
#endif
        auto* serviceRow = new QHBoxLayout;
        serviceRow->addWidget(btnOpenSerial);
        serviceRow->addWidget(btnCloseSerial);
        serviceRow->addWidget(serviceState);
        serviceRow->addStretch();

        serialEditors->push_back(SerialEditor{channel.key, portName, baudRate, format, frameSize,
                                              serviceState, btnOpenSerial, btnCloseSerial});

        form->addRow("Port", portName);
        form->addRow("Baud", baudRate);
        form->addRow("Format", format);
        form->addRow("Frames", frameSize);
        form->addRow("Service", serviceRow);

        connect(btnOpenSerial, &QPushButton::clicked,
                [this, key = channel.key] { m_vm.openSerialChannel(key); });
        connect(btnCloseSerial, &QPushButton::clicked,
                [this, key = channel.key] { m_vm.closeSerialChannel(key); });
        return group;
    };

    auto makeEndpointGroup = [this, endpointEditors, makeIpEdit,
                              makePortSpin](const NetworkEndpointState& endpoint) {
        auto* group = new QGroupBox(endpoint.displayName);
        auto* form = new QFormLayout(group);
        auto* localIp = makeIpEdit(endpoint.localIp);
        auto* localPort = makePortSpin(0, endpoint.localPort);
        auto* remoteIp = makeIpEdit(endpoint.remoteIp);
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
                [this, endpointEditors, key = endpoint.key] {
                    for (const auto& editor : *endpointEditors) {
                        if (editor.key == key) {
                            m_vm.applyNetworkEndpointConfig(
                                editor.key, editor.localIp->text(), editor.localPort->value(),
                                editor.remoteIp->text(), editor.remotePort->value());
                            return;
                        }
                    }
                });
        if (endpoint.canOpen) {
            connect(btnOpenEndpoint, &QPushButton::clicked,
                    [this, key = endpoint.key] { m_vm.openNetworkEndpoint(key); });
            connect(btnCloseEndpoint, &QPushButton::clicked,
                    [this, key = endpoint.key] { m_vm.closeNetworkEndpoint(key); });
        }
        return group;
    };

    layout->addWidget(new QLabel("Serial Channels"));
    auto* serialGrid = new QGridLayout;
    const auto serialChannels = m_vm.serialChannelConfigs();
    for (int index = 0; index < static_cast<int>(serialChannels.size()); ++index) {
        serialGrid->addWidget(makeSerialGroup(serialChannels[static_cast<std::size_t>(index)]),
                              index / 2, index % 2);
    }
    layout->addLayout(serialGrid);

    layout->addWidget(new QLabel("UDP Endpoints"));
    auto* endpointGrid = new QGridLayout;
    const auto endpoints = m_vm.networkEndpointConfigs();
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

    auto refreshEndpoints = [this, endpointEditors] {
        const auto currentEndpoints = m_vm.networkEndpointConfigs();
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
    auto refreshSerialChannels = [this, serialEditors] {
        const auto currentChannels = m_vm.serialChannelConfigs();
        for (const auto& channel : currentChannels) {
            for (const auto& editor : *serialEditors) {
                if (editor.key == channel.key) {
                    editor.portName->setText(channel.portName);
                    editor.baudRate->setText(QString::number(channel.baudRate));
                    editor.format->setText(
                        QString("%1 data / %2 stop").arg(channel.dataBits).arg(channel.stopBits));
                    editor.frameSize->setText(QString("rx %1 / tx %2")
                                                  .arg(channel.recvFrameSize)
                                                  .arg(channel.sendFrameSize));
                    editor.serviceState->setText(
                        channel.isRegistered
                            ? (channel.isOpen ? "Service: opened" : "Service: closed")
                            : "Service: not registered");
                    editor.openButton->setEnabled(channel.isRegistered && !channel.isOpen);
                    editor.closeButton->setEnabled(channel.isRegistered && channel.isOpen);
                }
            }
        }
    };
    auto refreshOpenState = [this, btnOpenDataExchange, btnCloseDataExchange, dataExchangeState] {
        const auto isOpen = m_vm.isDataExchangeOpen();
        btnOpenDataExchange->setEnabled(!isOpen);
        btnCloseDataExchange->setEnabled(isOpen);
        dataExchangeState->setText(isOpen ? "Data exchange: opened" : "Data exchange: closed");
    };

    connect(btnOpenDataExchange, &QPushButton::clicked, &m_vm, &ViewModel::openDataExchange);
    connect(btnCloseDataExchange, &QPushButton::clicked, &m_vm, &ViewModel::closeDataExchange);
    connect(&m_vm, &ViewModel::serialChannelsChanged, refreshSerialChannels);
    connect(&m_vm, &ViewModel::serialChannelStateChanged,
            [refreshSerialChannels](const QString&, bool) { refreshSerialChannels(); });
    connect(&m_vm, &ViewModel::networkEndpointsChanged, refreshEndpoints);
    connect(&m_vm, &ViewModel::dataExchangeOpenChanged, refreshOpenState);

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
