#ifndef UI_INITDLG_H
#define UI_INITDLG_H

#include <QDialog>
#include <QTimer>
#include "GlobalParameter.h"

namespace Ui {
class UI_InitDlg;
}

class UI_InitDlg : public QDialog
{
    Q_OBJECT

public:
    explicit UI_InitDlg(QWidget *parent = nullptr);
    ~UI_InitDlg();

private slots:
    void on_UITimerOut(void);

private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    Ui::UI_InitDlg *ui;
    QTimer* m_qtimerUI;
    float m_fProgress;
    float m_fInitTime;
    unsigned int m_uiTimerCnt;
};

#endif // UI_INITDLG_H
