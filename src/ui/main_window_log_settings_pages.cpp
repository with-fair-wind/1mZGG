#include "dss/ui/main_window.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include "dss/ui/log_palette.h"

#ifdef DSS_HAS_ELA
#include <ElaComboBox.h>
#endif

namespace Dss::Ui {

namespace {
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


}  // namespace
void MainWindow::setupAnalysisPage() {
    m_analysisPage = new QWidget;
    auto* layout = new QVBoxLayout(m_analysisPage);
    layout->addWidget(new QLabel("Analysis / Curve Charts"));
    // TODO: integrate ElaCharts or QChartView for distance curves
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


}  // namespace Dss::Ui