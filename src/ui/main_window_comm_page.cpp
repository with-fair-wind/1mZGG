#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <memory>
#include <vector>

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaPushButton.h>
#endif

namespace Dss::Ui {
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


}  // namespace Dss::Ui