#include "UI_InitDlg.h"
#include "ui_UI_InitDlg.h"

UI_InitDlg::UI_InitDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UI_InitDlg)
{
    m_pGParam = GlobalParameter::GetInstance();
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(this->width(), this->height());
    ui->setupUi(this);
    m_qtimerUI = new QTimer(this);
    connect(m_qtimerUI, SIGNAL(timeout()), this, SLOT(on_UITimerOut()));
    m_qtimerUI->start(100);
    m_fProgress = 0;
    m_fInitTime = 3.1;
    m_uiTimerCnt = 0;
    ui->progressBar->setValue(m_fProgress);
}

UI_InitDlg::~UI_InitDlg()
{
    delete ui;
}

#include <QDebug>
void UI_InitDlg::on_UITimerOut(void)
{
    if (m_uiTimerCnt++ == uint(m_fInitTime*10))
        m_qtimerUI->stop();
    ui->progressBar->setValue(uint(float(m_uiTimerCnt)/(m_fInitTime*10.0)*100));
}
