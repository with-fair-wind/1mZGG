#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <expected>
#include <string>

namespace Dss::Ui {

/// 系统初始化进度对话框，展示各模块加载状态
class InitDialog : public QDialog {
    Q_OBJECT

public:
    explicit InitDialog(QWidget* parent = nullptr);

    /**
     * @brief 记录某模块的初始化结果
     * @param module 模块名称
     * @param success 是否成功
     */
    void setStatus(const QString& module, bool success);
    /**
     * @brief 更新整体初始化进度
     * @param value 进度值（0–100）
     */
    void setProgress(int value);

signals:
    /// 初始化完成（全部模块成功时 success 为 true）
    void initComplete(bool success);

private:
    QLabel* m_statusLabel = nullptr;      ///< 汇总状态标签
    QProgressBar* m_progress = nullptr;   ///< 进度条
    QVBoxLayout* m_statusList = nullptr;  ///< 各模块状态列表布局
    int m_moduleCount = 0;                ///< 已报告模块总数
    int m_successCount = 0;               ///< 成功模块数
};

}  // namespace Dss::Ui
