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
#include "dss/ui/data_exchange_view_model.h"
#include "dss/ui/network_view_model.h"
#include "dss/ui/wheel_guarded_spin_box.h"

#ifdef DSS_HAS_ELA
#include <ElaPushButton.h>
#endif

namespace Dss::Ui {
namespace {
struct EndpointEditor {
    QString key;
    QLineEdit* localIp;
    QSpinBox* localPort;
    QLineEdit* remoteIp;
    QSpinBox* remotePort;
    QLabel* serviceState;
    QPushButton* openButton;
    QPushButton* closeButton;
    bool canOpen;
};

auto makePortSpin(int value, QWidget* parent) -> QSpinBox* {
    auto* spin = new WheelGuardedSpinBox(parent);
    spin->setRange(0, 65535);
    spin->setValue(value);
    return spin;
}
}  // namespace

auto createNetworkEndpointsPanel(NetworkViewModel& network, DataExchangeViewModel& dataExchange,
                                 QWidget* parent) -> QWidget* {
    auto* panel = new QWidget(parent);
    panel->setObjectName("network_endpoints_panel");
    auto* layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel("UDP Endpoints", panel));
    auto* grid = new QGridLayout;
    layout->addLayout(grid);
    auto editors = std::make_shared<std::vector<EndpointEditor>>();

    const auto endpoints = network.networkEndpointConfigs();
    for (int index = 0; index < static_cast<int>(endpoints.size()); ++index) {
        const auto& endpoint = endpoints[static_cast<std::size_t>(index)];
        auto* group = new QGroupBox(endpoint.displayName, panel);
        auto* form = new QFormLayout(group);
        auto* localIp = new QLineEdit(endpoint.localIp, group);
        auto* localPort = makePortSpin(endpoint.localPort, group);
        auto* remoteIp = new QLineEdit(endpoint.remoteIp, group);
        auto* remotePort = makePortSpin(endpoint.remotePort, group);
        localIp->setClearButtonEnabled(true);
        remoteIp->setClearButtonEnabled(true);
#ifdef DSS_HAS_ELA
        auto* applyButton = new ElaPushButton("Apply", group);
#else
        auto* applyButton = new QPushButton("Apply", group);
#endif
        auto* serviceState = new QLabel(group);
        QPushButton* openButton = nullptr;
        QPushButton* closeButton = nullptr;
        auto* serviceRow = new QHBoxLayout;
        if (endpoint.canOpen) {
#ifdef DSS_HAS_ELA
            openButton = new ElaPushButton("Open", group);
            closeButton = new ElaPushButton("Close", group);
#else
            openButton = new QPushButton("Open", group);
            closeButton = new QPushButton("Close", group);
#endif
            serviceRow->addWidget(openButton);
            serviceRow->addWidget(closeButton);
        }
        serviceRow->addWidget(serviceState);
        serviceRow->addStretch();
        form->addRow("Local IP", localIp);
        form->addRow("Local Port", localPort);
        form->addRow("Remote IP", remoteIp);
        form->addRow("Remote Port", remotePort);
        form->addRow(applyButton);
        form->addRow("Service", serviceRow);
        editors->push_back({endpoint.key, localIp, localPort, remoteIp, remotePort, serviceState,
                            openButton, closeButton, endpoint.canOpen});

        QObject::connect(applyButton, &QPushButton::clicked, panel,
                         [&network, editors, key = endpoint.key] {
                             for (const auto& editor : *editors) {
                                 if (editor.key == key) {
                                     network.applyNetworkEndpointConfig(
                                         key, editor.localIp->text(), editor.localPort->value(),
                                         editor.remoteIp->text(), editor.remotePort->value());
                                     return;
                                 }
                             }
                         });
        if (endpoint.canOpen) {
            QObject::connect(openButton, &QPushButton::clicked, panel,
                             [&network, key = endpoint.key] { network.openNetworkEndpoint(key); });
            QObject::connect(closeButton, &QPushButton::clicked, panel,
                             [&network, key = endpoint.key] { network.closeNetworkEndpoint(key); });
        }
        grid->addWidget(group, index / 2, index % 2);
    }

    auto* actions = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* openExchange = new ElaPushButton("Open UDP", panel);
    auto* closeExchange = new ElaPushButton("Close UDP", panel);
#else
    auto* openExchange = new QPushButton("Open UDP", panel);
    auto* closeExchange = new QPushButton("Close UDP", panel);
#endif
    auto* exchangeState = new QLabel(panel);
    actions->addWidget(openExchange);
    actions->addWidget(closeExchange);
    actions->addWidget(exchangeState);
    actions->addStretch();
    layout->addLayout(actions);

    auto refreshEndpoints = [&network, editors] {
        for (const auto& endpoint : network.networkEndpointConfigs()) {
            for (const auto& editor : *editors) {
                if (editor.key != endpoint.key) {
                    continue;
                }
                editor.localIp->setText(endpoint.localIp);
                editor.localPort->setValue(endpoint.localPort);
                editor.remoteIp->setText(endpoint.remoteIp);
                editor.remotePort->setValue(endpoint.remotePort);
                editor.serviceState->setText(
                    endpoint.canOpen ? (endpoint.isOpen ? "Service: opened" : "Service: closed")
                                     : "Service: use Data Exchange controls");
                if (editor.openButton != nullptr) {
                    editor.openButton->setEnabled(endpoint.canOpen && !endpoint.isOpen);
                    editor.closeButton->setEnabled(endpoint.canOpen && endpoint.isOpen);
                }
            }
        }
    };
    auto refreshExchange = [&dataExchange, openExchange, closeExchange, exchangeState] {
        const auto open = dataExchange.isDataExchangeOpen();
        openExchange->setEnabled(!open);
        closeExchange->setEnabled(open);
        exchangeState->setText(open ? "Data exchange: opened" : "Data exchange: closed");
    };
    QObject::connect(openExchange, &QPushButton::clicked, &dataExchange,
                     &DataExchangeViewModel::openDataExchange);
    QObject::connect(closeExchange, &QPushButton::clicked, &dataExchange,
                     &DataExchangeViewModel::closeDataExchange);
    QObject::connect(&network, &NetworkViewModel::networkEndpointsChanged, panel, refreshEndpoints);
    QObject::connect(&network, &NetworkViewModel::dataExchangeEndpointsChanged, panel,
                     refreshEndpoints);
    QObject::connect(&dataExchange, &DataExchangeViewModel::dataExchangeEndpointsChanged, panel,
                     refreshEndpoints);
    QObject::connect(&dataExchange, &DataExchangeViewModel::dataExchangeOpenChanged, panel,
                     refreshExchange);
    refreshEndpoints();
    refreshExchange();
    return panel;
}

}  // namespace Dss::Ui
