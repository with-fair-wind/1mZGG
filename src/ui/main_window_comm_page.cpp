#include <QLabel>
#include <QVBoxLayout>

#include "dss/ui/communication_panels.h"
#include "dss/ui/main_window.h"

namespace Dss::Ui {

void MainWindow::setupCommStatusPage() {
    m_commPage = new QWidget;
    auto* layout = new QVBoxLayout(m_commPage);
    layout->addWidget(new QLabel("Communication Status Monitor", m_commPage));
    layout->addWidget(createSerialChannelsPanel(m_mainViewModel.serialPorts(), m_commPage));
    layout->addWidget(createSerialCommandsPanel(m_mainViewModel.serialPorts(), m_commPage));
    layout->addWidget(createNetworkEndpointsPanel(m_mainViewModel.network(),
                                                  m_mainViewModel.dataExchange(), m_commPage));
    layout->addStretch();
}

}  // namespace Dss::Ui
