#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include "dss/ui/communication_panels.h"
#include "dss/ui/serial_port_view_model.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaPushButton.h>
#endif

namespace Dss::Ui {
namespace {
auto makeIntSpin(int minimum, int maximum, int value, QWidget* parent) -> QSpinBox* {
    auto* spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

auto makeDoubleSpin(double minimum, double maximum, QWidget* parent) -> QDoubleSpinBox* {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setDecimals(3);
    return spin;
}
}  // namespace

auto createSerialCommandsPanel(SerialPortViewModel& viewModel, QWidget* parent) -> QWidget* {
    auto* panel = new QWidget(parent);
    panel->setObjectName("serial_commands_panel");
    auto* layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel("Serial Commands", panel));
    auto* grid = new QGridLayout;
    layout->addLayout(grid);

    auto* exposureGroup = new QGroupBox("Exposure Command", panel);
    auto* exposureForm = new QFormLayout(exposureGroup);
#ifdef DSS_HAS_ELA
    auto* freeRun = new ElaCheckBox("Free Run", exposureGroup);
    auto* exposureSend = new ElaPushButton("Send", exposureGroup);
#else
    auto* freeRun = new QCheckBox("Free Run", exposureGroup);
    auto* exposureSend = new QPushButton("Send", exposureGroup);
#endif
    auto* frameFrequency = makeIntSpin(0, 255, 1, exposureGroup);
    auto* delayTicks = makeIntSpin(0, 0xFF'FFFF, 0, exposureGroup);
    exposureForm->addRow("Trigger", freeRun);
    exposureForm->addRow("Frame Code", frameFrequency);
    exposureForm->addRow("Delay Ticks", delayTicks);
    exposureForm->addRow(exposureSend);
    QObject::connect(exposureSend, &QPushButton::clicked, panel,
                     [&viewModel, freeRun, frameFrequency, delayTicks] {
                         viewModel.sendExposureCommand(
                             freeRun->isChecked(), frameFrequency->value(), delayTicks->value());
                     });

    auto* servoGroup = new QGroupBox("Servo Correction", panel);
    auto* servoForm = new QFormLayout(servoGroup);
#ifdef DSS_HAS_ELA
    auto* servoDistanceValid = new ElaCheckBox("Distance", servoGroup);
    auto* servoSpeedValid = new ElaCheckBox("Speed", servoGroup);
    auto* servoSend = new ElaPushButton("Send", servoGroup);
#else
    auto* servoDistanceValid = new QCheckBox("Distance", servoGroup);
    auto* servoSpeedValid = new QCheckBox("Speed", servoGroup);
    auto* servoSend = new QPushButton("Send", servoGroup);
#endif
    auto* servoDistanceX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, servoGroup);
    auto* servoDistanceY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, servoGroup);
    auto* servoSpeedX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, servoGroup);
    auto* servoSpeedY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, servoGroup);
    auto* servoMode = makeIntSpin(0, 255, 0x19, servoGroup);
    servoForm->addRow("Distance Valid", servoDistanceValid);
    servoForm->addRow("Distance X", servoDistanceX);
    servoForm->addRow("Distance Y", servoDistanceY);
    servoForm->addRow("Speed Valid", servoSpeedValid);
    servoForm->addRow("Speed X", servoSpeedX);
    servoForm->addRow("Speed Y", servoSpeedY);
    servoForm->addRow("Mode", servoMode);
    servoForm->addRow(servoSend);
    QObject::connect(servoSend, &QPushButton::clicked, panel,
                     [&viewModel, servoDistanceValid, servoSpeedValid, servoDistanceX,
                      servoDistanceY, servoSpeedX, servoSpeedY, servoMode] {
                         viewModel.sendServoCorrection(
                             servoDistanceValid->isChecked(), servoSpeedValid->isChecked(),
                             servoDistanceX->value(), servoDistanceY->value(), servoSpeedX->value(),
                             servoSpeedY->value(), servoMode->value());
                     });

    auto* masterGroup = new QGroupBox("Master Control Status", panel);
    auto* masterForm = new QFormLayout(masterGroup);
#ifdef DSS_HAS_ELA
    auto* masterDistanceValid = new ElaCheckBox("Distance", masterGroup);
    auto* masterSpeedValid = new ElaCheckBox("Speed", masterGroup);
    auto* masterSend = new ElaPushButton("Send", masterGroup);
#else
    auto* masterDistanceValid = new QCheckBox("Distance", masterGroup);
    auto* masterSpeedValid = new QCheckBox("Speed", masterGroup);
    auto* masterSend = new QPushButton("Send", masterGroup);
#endif
    auto* azimuth = makeDoubleSpin(0.0, 360.0, masterGroup);
    auto* elevation = makeDoubleSpin(0.0, 360.0, masterGroup);
    auto* masterDistanceX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, masterGroup);
    auto* masterDistanceY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, masterGroup);
    auto* masterSpeedX = makeDoubleSpin(-1'000'000.0, 1'000'000.0, masterGroup);
    auto* masterSpeedY = makeDoubleSpin(-1'000'000.0, 1'000'000.0, masterGroup);
    auto* masterMode = makeIntSpin(0, 255, 0x19, masterGroup);
    masterForm->addRow("Azimuth", azimuth);
    masterForm->addRow("Elevation", elevation);
    masterForm->addRow("Distance Valid", masterDistanceValid);
    masterForm->addRow("Distance X", masterDistanceX);
    masterForm->addRow("Distance Y", masterDistanceY);
    masterForm->addRow("Speed Valid", masterSpeedValid);
    masterForm->addRow("Speed X", masterSpeedX);
    masterForm->addRow("Speed Y", masterSpeedY);
    masterForm->addRow("Mode", masterMode);
    masterForm->addRow(masterSend);
    QObject::connect(masterSend, &QPushButton::clicked, panel,
                     [&viewModel, azimuth, elevation, masterDistanceValid, masterSpeedValid,
                      masterDistanceX, masterDistanceY, masterSpeedX, masterSpeedY, masterMode] {
                         const auto now = QDateTime::currentDateTime();
                         const auto date = now.date();
                         const auto time = now.time();
                         viewModel.sendMasterControlStatus(
                             date.year(), date.month(), date.day(), time.hour(), time.minute(),
                             time.second(), time.msec(), azimuth->value(), elevation->value(),
                             masterDistanceValid->isChecked(), masterSpeedValid->isChecked(),
                             masterDistanceX->value(), masterDistanceY->value(),
                             masterSpeedX->value(), masterSpeedY->value(), masterMode->value());
                     });

    grid->addWidget(exposureGroup, 0, 0);
    grid->addWidget(servoGroup, 0, 1);
    grid->addWidget(masterGroup, 1, 0, 1, 2);
    return panel;
}

}  // namespace Dss::Ui
