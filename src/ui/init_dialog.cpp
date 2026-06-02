#include "dss/ui/init_dialog.h"

#ifdef DSS_HAS_ELA
#include <ElaProgressBar.h>
#include <ElaPushButton.h>
#include <ElaText.h>
#else
#include <QPushButton>
#endif

namespace Dss::Ui {

InitDialog::InitDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("DSS_QT - System Initialization");
    setMinimumSize(400, 300);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel("Initializing modules..."));

#ifdef DSS_HAS_ELA
    m_progress = new ElaProgressBar;
#else
    m_progress = new QProgressBar;
#endif
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    mainLayout->addWidget(m_progress);

    m_statusList = new QVBoxLayout;
    mainLayout->addLayout(m_statusList);
    mainLayout->addStretch();

    m_statusLabel = new QLabel("Starting...");
    mainLayout->addWidget(m_statusLabel);
}

void InitDialog::setStatus(const QString& module, bool success) {
    auto* label = new QLabel(QString("%1 %2").arg(success ? "✓" : "✗").arg(module));
    label->setStyleSheet(success ? "color: green;" : "color: red; font-weight: bold;");
    m_statusList->addWidget(label);

    ++m_moduleCount;
    if (success) {
        ++m_successCount;
    }

    m_statusLabel->setText(QString("Modules: %1/%2 OK").arg(m_successCount).arg(m_moduleCount));
}

void InitDialog::setProgress(int value) {
    m_progress->setValue(value);
    if (value >= 100) {
        Q_EMIT initComplete(m_successCount == m_moduleCount);
    }
}

}  // namespace Dss::Ui
