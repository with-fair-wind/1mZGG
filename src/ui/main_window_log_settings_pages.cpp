#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <memory>

#include "dss/ui/log_palette.h"
#include "dss/ui/main_window.h"

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
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto& settings = m_mainViewModel.settings();
    const auto snapshot = settings.snapshot();

    auto* dataRoot = new QLineEdit(snapshot.dataRoot, m_settingsPage);
    dataRoot->setObjectName("settings_data_root");
    auto* logEnabled = new QCheckBox("Enable file logging", m_settingsPage);
    logEnabled->setChecked(snapshot.logEnabled);
    auto* logPath = new QLineEdit(snapshot.logFilePath, m_settingsPage);
    logPath->setObjectName("settings_log_path");
    auto* logSize = new QSpinBox(m_settingsPage);
    logSize->setRange(1, 2'000'000'000);
    logSize->setValue(static_cast<int>(snapshot.logMaxFileSizeBytes));
    auto* logFiles = new QSpinBox(m_settingsPage);
    logFiles->setRange(1, 100);
    logFiles->setValue(static_cast<int>(snapshot.logMaxFiles));
    auto* imageWidth = new QSpinBox(m_settingsPage);
    imageWidth->setRange(1, 100'000);
    imageWidth->setValue(snapshot.imageWidth);
    auto* imageHeight = new QSpinBox(m_settingsPage);
    imageHeight->setRange(1, 100'000);
    imageHeight->setValue(snapshot.imageHeight);
    auto* pixelScale = new QDoubleSpinBox(m_settingsPage);
    pixelScale->setRange(0.000001, 1'000'000.0);
    pixelScale->setDecimals(8);
    pixelScale->setValue(snapshot.pixelScale);
    auto* saveButton = new QPushButton("Save", m_settingsPage);
    saveButton->setObjectName("settings_save");
    auto* status = new QLabel(m_settingsPage);

    form->addRow("Data Root", dataRoot);
    form->addRow("File Logging", logEnabled);
    form->addRow("Log File", logPath);
    form->addRow("Log Size (bytes)", logSize);
    form->addRow("Log Files", logFiles);
    form->addRow("Image Width", imageWidth);
    form->addRow("Image Height", imageHeight);
    form->addRow("Pixel Scale", pixelScale);
    form->addRow(saveButton);
    form->addRow("Status", status);

    connect(saveButton, &QPushButton::clicked, m_settingsPage,
            [&settings, dataRoot, logEnabled, logPath, logSize, logFiles, imageWidth, imageHeight,
             pixelScale, status] {
                const SettingsSnapshot updated{
                    .dataRoot = dataRoot->text(),
                    .logEnabled = logEnabled->isChecked(),
                    .logFilePath = logPath->text(),
                    .logMaxFileSizeBytes = static_cast<std::size_t>(logSize->value()),
                    .logMaxFiles = static_cast<std::size_t>(logFiles->value()),
                    .imageWidth = imageWidth->value(),
                    .imageHeight = imageHeight->value(),
                    .pixelScale = pixelScale->value(),
                };
                const auto result = settings.save(updated);
                status->setText(result ? "Saved" : result.error());
            });
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
    filterRow->addWidget(new QLabel("Search:"));
    auto* search = new QLineEdit;
    search->setObjectName("log_search");
    search->setClearButtonEnabled(true);
    filterRow->addWidget(search, 1);
    auto* exportButton = new QPushButton("Export");
    exportButton->setObjectName("log_export");
    filterRow->addWidget(exportButton);
    layout->addLayout(filterRow);

    auto* logText = new QTextEdit;
    logText->setReadOnly(true);
    logText->setLineWrapMode(QTextEdit::NoWrap);
    auto currentEntries = std::make_shared<QStringList>();
    auto refreshLogText = [logText, search, currentEntries](const QStringList& entries) {
        *currentEntries = entries;
        QStringList filtered;
        const auto query = search->text().trimmed();
        for (const auto& entry : entries) {
            if (query.isEmpty() || entry.contains(query, Qt::CaseInsensitive)) {
                filtered.push_back(entry);
            }
        }
        renderColoredLogEntries(*logText, filtered);
    };
    connect(&logs, &LogViewModel::logEntriesChanged, logText, refreshLogText);
    connect(levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &logs,
            &LogViewModel::setLogMinimumLevel);
    connect(search, &QLineEdit::textChanged, logText,
            [refreshLogText, currentEntries](const QString&) { refreshLogText(*currentEntries); });
    connect(exportButton, &QPushButton::clicked, m_logPage, [logText, this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export Logs", "dss-ui.log",
                                                       "Log Files (*.log *.txt)");
        if (path.isEmpty()) {
            return;
        }
        QFile output(path);
        if (output.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream stream(&output);
            stream << logText->toPlainText() << '\n';
        }
    });
    refreshLogText(logs.visibleLogEntries());
    layout->addWidget(logText);
}

}  // namespace Dss::Ui
