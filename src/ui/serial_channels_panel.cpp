#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <vector>

#include "dss/ui/communication_panels.h"
#include "dss/ui/serial_port_view_model.h"
#include "dss/ui/wheel_guarded_spin_box.h"

#ifdef DSS_HAS_ELA
#include <ElaPushButton.h>
#endif

namespace Dss::Ui {
namespace {
struct SerialEditor {
    QString key;
    QLineEdit* portName;
    QSpinBox* baudRate;
    QSpinBox* dataBits;
    QSpinBox* stopBits;
    QLabel* frameSize;
    QLabel* serviceState;
    QPushButton* applyButton;
    QPushButton* openButton;
    QPushButton* closeButton;
};
}  // namespace

auto createSerialChannelsPanel(SerialPortViewModel& viewModel, QWidget* parent) -> QWidget* {
    auto* panel = new QWidget(parent);
    panel->setObjectName("serial_channels_panel");
    auto* layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel("Serial Channels", panel));
    auto* grid = new QGridLayout;
    layout->addLayout(grid);

    auto editors = std::make_shared<std::vector<SerialEditor>>();
    const auto channels = viewModel.serialChannelConfigs();
    for (int index = 0; index < static_cast<int>(channels.size()); ++index) {
        const auto& channel = channels[static_cast<std::size_t>(index)];
        auto* group = new QGroupBox(channel.displayName, panel);
        auto* form = new QFormLayout(group);
        auto* portName = new QLineEdit(channel.portName, group);
        portName->setClearButtonEnabled(true);
        auto* baudRate = new WheelGuardedSpinBox(group);
        baudRate->setRange(1, 4'000'000);
        auto* dataBits = new WheelGuardedSpinBox(group);
        dataBits->setRange(5, 8);
        auto* stopBits = new WheelGuardedSpinBox(group);
        stopBits->setRange(1, 2);
        auto* frameSize = new QLabel(group);
        auto* serviceState = new QLabel(group);
#ifdef DSS_HAS_ELA
        auto* applyButton = new ElaPushButton("Apply", group);
        auto* openButton = new ElaPushButton("Open", group);
        auto* closeButton = new ElaPushButton("Close", group);
#else
        auto* applyButton = new QPushButton("Apply", group);
        auto* openButton = new QPushButton("Open", group);
        auto* closeButton = new QPushButton("Close", group);
#endif
        auto* serviceRow = new QHBoxLayout;
        serviceRow->addWidget(openButton);
        serviceRow->addWidget(closeButton);
        serviceRow->addWidget(serviceState);
        serviceRow->addStretch();
        form->addRow("Port", portName);
        form->addRow("Baud", baudRate);
        form->addRow("Data Bits", dataBits);
        form->addRow("Stop Bits", stopBits);
        form->addRow("Frames", frameSize);
        form->addRow(applyButton);
        form->addRow("Service", serviceRow);

        editors->push_back({channel.key, portName, baudRate, dataBits, stopBits, frameSize,
                            serviceState, applyButton, openButton, closeButton});
        QObject::connect(applyButton, &QPushButton::clicked, panel,
                         [&viewModel, editors, key = channel.key] {
                             for (const auto& editor : *editors) {
                                 if (editor.key == key) {
                                     viewModel.applySerialChannelConfig(
                                         key, editor.portName->text(), editor.baudRate->value(),
                                         editor.dataBits->value(), editor.stopBits->value());
                                     return;
                                 }
                             }
                         });
        QObject::connect(openButton, &QPushButton::clicked, panel,
                         [&viewModel, key = channel.key] { viewModel.openSerialChannel(key); });
        QObject::connect(closeButton, &QPushButton::clicked, panel,
                         [&viewModel, key = channel.key] { viewModel.closeSerialChannel(key); });
        grid->addWidget(group, index / 2, index % 2);
    }

    auto refresh = [&viewModel, editors] {
        for (const auto& channel : viewModel.serialChannelConfigs()) {
            for (const auto& editor : *editors) {
                if (editor.key != channel.key) {
                    continue;
                }
                editor.portName->setText(channel.portName);
                editor.baudRate->setValue(channel.baudRate);
                editor.dataBits->setValue(channel.dataBits);
                editor.stopBits->setValue(channel.stopBits);
                editor.frameSize->setText(
                    QString("rx %1 / tx %2").arg(channel.recvFrameSize).arg(channel.sendFrameSize));
                editor.serviceState->setText(
                    channel.isRegistered ? (channel.isOpen ? "Service: opened" : "Service: closed")
                                         : "Service: not registered");
                editor.openButton->setEnabled(channel.isRegistered && !channel.isOpen);
                editor.closeButton->setEnabled(channel.isRegistered && channel.isOpen);
            }
        }
    };
    QObject::connect(&viewModel, &SerialPortViewModel::serialChannelsChanged, panel, refresh);
    QObject::connect(&viewModel, &SerialPortViewModel::serialChannelStateChanged, panel,
                     [refresh](const QString&, bool) { refresh(); });
    refresh();
    return panel;
}

}  // namespace Dss::Ui
