#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <expected>
#include <string>

namespace Dss::Ui {

class InitDialog : public QDialog {
    Q_OBJECT

public:
    explicit InitDialog(QWidget* parent = nullptr);

    void setStatus(const QString& module, bool success);
    void setProgress(int value);

signals:
    void initComplete(bool success);

private:
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QVBoxLayout* m_statusList = nullptr;
    int m_moduleCount = 0;
    int m_successCount = 0;
};

}  // namespace Dss::Ui
