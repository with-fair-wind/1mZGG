#include "UI_CtrlPad.h"

UI_CtrlPad::UI_CtrlPad(QWidget *parent)
        : QMainWindow(parent)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGrabber = (Grabber*)m_pGParam->m_SGrabberData.pvoidGrabber;
    m_pImageReplayer = (ImageReplayer*)m_pGParam->m_SImageReplayerData.pvoidImageReplayer;
    m_pImageProcessor = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
//    m_pDataProcessor = (DataProcessor*)m_pGParam->m_SDataProcessorData.pvoidDataProcessor;
    m_pImageStorage = (ImageStorage*)m_pGParam->m_SImageStorageData.pvoidImageStorage;
    m_pTrackDataStorage = (TrackDataStorage*)m_pGParam->m_STrackDataStorageData.pvoidTrackDataStorage;
    m_pLog = (MyLog*)m_pGParam->m_SLog.pvoidThis;
    m_fExpTime = 0;
    m_fGain = 0;
    m_qstrPathReplay = "";
    m_bMCCtrlSetting = false;
    m_bParamsLoading = false;
    UIInit();
    unsigned int uiBlobDown = ui.lineEdit_BlobDown->text().toUInt();
    unsigned int uiBlobUp = ui.lineEdit_BlobUp->text().toUInt();
    m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
    unsigned int uiScaleDown = ui.lineEdit_ScaleDown->text().toUInt();
    unsigned int uiScaleUp = ui.lineEdit_ScaleUp->text().toUInt();
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    m_pImageProcessor->SetScale(bAutoScale, uiScaleUp, uiScaleDown);
    m_pImageProcessor->SetDispEnhance(ui.checkBox_EnhanceDisp->isChecked());
    m_pImageProcessor->SetCenterMode(!(ui.comboBox_TrackMode_2->currentIndex() == 1));
    m_qtimerUI = new QTimer(this);
    connect(m_qtimerUI, SIGNAL(timeout()), this, SLOT(on_UITimerOut()));
    m_qtimerUI->start(100);

    ui.checkBox_MCCtrl->setChecked(true);
    on_checkBox_MCCtrl_clicked();
}
#include <QDebug>
UI_CtrlPad::~UI_CtrlPad()
{
    qDebug() << "UI_CtrlPad";
}

void UI_CtrlPad::UIInit(void)
{
    setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    ui.setupUi(this);
//    setFixedSize(this->width(), this->height());
    setGeometry(1620, 80, this->width(), this->height());
    ui.checkBox_MCCtrl->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_SpdValid->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_AutoScale->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_DistValid->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_ImageGrab->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_ImageSave->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_TrackProc->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_TrackSave->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_DrawStarMap->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_MatchTimeSave->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_EnhanceDisp->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
    ui.checkBox_TrackInfoEN->setStyleSheet("QCheckBox::indicator{width: 18px; height: 18px;}");
//    ui.checkBox_Dark->setVisible(false);
    ui.checkBox_Dark->setVisible(true);
    ui.comboBox_ThreshBW->setVisible(true);
    ui.comboBox_ThreshBW->setCurrentIndex(3);   // 8.0
    m_pImageProcessor->SetStdRatio(8.0);
    ui.label_BlobUp_3->setVisible(true);
    ui.comboBox_FrameFreq->setCurrentIndex(1);  // 5.0s
    m_pGrabber->SetFrameInterval(5000.0);
    ui.comboBox_TrackMode_2->setCurrentIndex(1);
    ui.checkBox_ImageGrab->setChecked(false);
    ui.checkBox_DistValid->setChecked(true);
    ui.checkBox_SpdValid->setChecked(true);
    ui.lineEdit_ImageSavePath->setText(m_pGParam->m_SPath.qstrDataStorePath);
    ui.checkBox_MatchTimeSave->setChecked(true);
    m_pImageStorage->SetMatchTimeStore(ui.checkBox_MatchTimeSave->isChecked());
    ui.checkBox_ImageSave->setChecked(false);
    ui.lineEdit_ImagePlayPath->setText("");
    ui.checkBox_AutoScale->setChecked(true);
    ui.checkBox_EnhanceDisp->setChecked(false);
    ui.checkBox_EnhanceDisp->setEnabled(m_pGParam->m_bDebugEN);
    ui.lineEdit_ScaleUp->setText(QString::number(5000));
    ui.lineEdit_ScaleUp->setEnabled(!ui.checkBox_AutoScale->isChecked());
    ui.lineEdit_ScaleDown->setText(QString::number(1000));
    ui.lineEdit_ScaleDown->setEnabled(!ui.checkBox_AutoScale->isChecked());
    ui.horizontalSlider_ScaleUp->setValue(5000);
    ui.horizontalSlider_ScaleUp->setEnabled(!ui.checkBox_AutoScale->isChecked());
    ui.horizontalSlider_ScaleDown->setValue(1000);
    ui.horizontalSlider_ScaleDown->setEnabled(!ui.checkBox_AutoScale->isChecked());
    ui.lineEdit_BlobDown->setText(QString::number(20));
    ui.lineEdit_BlobUp->setText(QString::number(15000));
    ui.checkBox_TrackSave->setChecked(false);
    ui.comboBox_TrackMode->setCurrentIndex(0);
    ui.checkBox_TrackInfoEN->setChecked(false);
    ui.textEdit_TrackInfo->setReadOnly(true);
    ui.checkBox_MCCtrl->setChecked(false);
    ui.radioButton_ZoomFit->setChecked(true);
    ui.textEdit_TrackInfo->setFrameShape(QFrame::StyledPanel);
    m_uiIndexTrackMode = 10000000;
    ui.tableWidget_TargetInfo->setColumnWidth(0, 80);
    ui.tableWidget_TargetInfo->setColumnWidth(1, 105);
    ui.tableWidget_TargetInfo->setColumnWidth(2, 160);
    ui.tableWidget_TargetInfo->setColumnWidth(3, 160);
    ui.tableWidget_TargetInfo->setColumnWidth(4, 80);
    ui.tableWidget_TargetInfo->setColumnWidth(5, 207);
    ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));

    ui.lineEdit_PythonExE->setText(m_pGParam->m_STrackParams.qstrExEPath);
    ui.lineEdit_pyPath->setText(m_pGParam->m_STrackParams.qstrPYPath);
    ui.lineEdit_RaThresh->setText("0.2");
    ui.lineEdit_DecThresh->setText("0.2");
    ui.lineEdit_RaSpdThresh->setText("2");
    ui.lineEdit_DecSpdThresh->setText("1");
    changeRaDecTrackParams();    
    ui.lineEdit_AddFrameNum->setText(QString::number(m_pGParam->m_SAddImage.uiAddFrameNum));
    ui.checkBox_LockDisp->setChecked(true);
    ui.checkBox_TrackAlgorithm->setChecked(false);
    ui.groupBox_3->setEnabled(false);

    QVector<QPair<QString, QString>> data = {
            {"x", "0.0"},
            {"y", "0.0"},
            {"xmin", "0.0"},
            {"xmax", "0.0"},
            {"ymin", "0.0"},
            {"ymax", "0.0"},
            {"area", "0.0"},
            {"flux", "0.0"},
            {"magIns", "0.0"}
        };
    ui.tableWidget_SourceInfo->setColumnCount(2);
    ui.tableWidget_SourceInfo->setHorizontalHeaderLabels(QStringList() << "参数" << "数值");
    ui.tableWidget_SourceInfo->setRowCount(data.size());
    ui.tableWidget_SourceInfo->horizontalHeader()->setStretchLastSection(true);

    for (int row = 0; row < data.size(); ++row)
    {
            QTableWidgetItem *keyItem = new QTableWidgetItem(data[row].first);
            QTableWidgetItem *valueItem = new QTableWidgetItem(data[row].second);
            ui.tableWidget_SourceInfo->setItem(row, 0, keyItem);
            ui.tableWidget_SourceInfo->setItem(row, 1, valueItem);
    }
    ui.tableWidget_SourceInfo->setEditTriggers(QAbstractItemView::NoEditTriggers);
    LoadPYFiles(m_pGParam->m_STrackParams.qstrPYPath);

    LoadParams();
    m_pImageProcessor->Init_TWDW();
}

void UI_CtrlPad::on_UITimerOut(void)
{
}

void UI_CtrlPad::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Escape:
            event->ignore();
            break;
        case Qt::Key_Enter:
            event->ignore();
            break;
        case Qt::Key_Space:
            event->ignore();
            break;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

void UI_CtrlPad::closeEvent(QCloseEvent* event)
{
    if (!m_pGrabber->GetGrabStatus())
    {
        qDebug() << "why??????????";
        emit SignalCloseCuard();
        QThread::msleep(500);
        emit SignalClose();
        QMainWindow::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}

void UI_CtrlPad::on_checkBox_DistValid_clicked()
{
    m_pGParam->m_SCommServoData.bDistValid = ui.checkBox_DistValid->isChecked();
}

void UI_CtrlPad::on_checkBox_SpdValid_clicked()
{
    m_pGParam->m_SCommServoData.bSpdValid = ui.checkBox_SpdValid->isChecked();
}

void UI_CtrlPad::on_checkBox_ImageGrab_clicked(void)
{
    if (!ui.checkBox_MCCtrl->isChecked())
    {
        if (ui.checkBox_ImageGrab->isChecked())
        {
            bool bGrabbing = m_pGrabber->GetGrabStatus();
            if(bGrabbing) m_pGrabber->PauseGrab();
            LoadParams();
            m_pImageReplayer->SetPause();
            m_pGrabber->ResumeGrab();
            ui.pushButton_PlayPathBrowse->setEnabled(false);
            ui.pushButton_Next->setEnabled(false);
            ui.pushButton_Pause->setEnabled(false);
            ui.pushButton_Previous->setEnabled(false);
            ui.pushButton_NextAuto->setEnabled(false);
            ui.pushButton_PreviousAuto->setEnabled(false);
            ui.pushButton_DistCurve->setEnabled(false);
            ui.pushButton_DistCurve_2->setEnabled(false);
            m_pLog->InfoMsg(QStringLiteral("用户开启图像采集."));
        }
        else
        {
            m_pGrabber->PauseGrab();
            ui.pushButton_PlayPathBrowse->setEnabled(true);
            ui.pushButton_Next->setEnabled(true);
            ui.pushButton_Pause->setEnabled(true);
            ui.pushButton_Previous->setEnabled(true);
            ui.pushButton_NextAuto->setEnabled(true);
            ui.pushButton_PreviousAuto->setEnabled(true);
            ui.pushButton_DistCurve->setEnabled(true);
            ui.pushButton_DistCurve_2->setEnabled(true);
//            ui.checkBox_ImageSave->setChecked(false);
//            m_pImageStorage->SetStore(false);
            m_pLog->InfoMsg(QStringLiteral("用户停止图像采集."));
        }
    }
}

void UI_CtrlPad::on_pushButton_SavePathBrowse_clicked(void)
{
    if (!ui.checkBox_ImageSave->isChecked())
    {
        QString qstrPath = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图像存储路径"), "/home/dps/DPS_DATA");
        QDir qDirMake;
        if (qDirMake.exists(qstrPath))
        {
            m_pGParam->m_SPath.qstrDataStorePath = qstrPath;
            ui.lineEdit_ImageSavePath->setText(qstrPath);
            QString qstrSub1 = qstrPath;
            QString qstrMsg = QStringLiteral("用户设置数据存储路径为") + qstrSub1 + ".";
            m_pLog->InfoMsg(qstrMsg);
        }
        else
        {
            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像存储路径无效.\n设置无效."));
            m_pLog->WarnMsg(QStringLiteral("用户设置数据存储路径失败."));
        }
    }
}

void UI_CtrlPad::on_checkBox_MatchTimeSave_clicked(void)
{
    if (!ui.checkBox_ImageSave->isChecked())
    {
        m_pImageStorage->SetMatchTimeStore(ui.checkBox_MatchTimeSave->isChecked());
        QString qstrSub1 = ui.checkBox_MatchTimeSave->isChecked() ? QStringLiteral("匹配时间存储") : QStringLiteral("不匹配时间存储");
        QString qstrMsg = QStringLiteral("用户设置") + qstrSub1 + ".";
        m_pLog->InfoMsg(qstrMsg);
    }
    else
    {
        ui.checkBox_MatchTimeSave->setChecked(!ui.checkBox_MatchTimeSave->isChecked());
        m_pLog->InfoMsg(QStringLiteral("图像正在存储.\n用户设置是否匹配时间存储失败."));
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像正在存储.\n设置无效."));
    }
}

void UI_CtrlPad::on_checkBox_ImageSave_clicked(void)
{
//    if (m_pGrabber->GetGrabStatus())
//    {
//        if (ui.checkBox_ImageSave->isChecked())
//        {
//            m_pLog->WarnMsg(QStringLiteral("用户设置图像存储成功."));
//        }
//        m_pImageStorage->SetStore(ui.checkBox_ImageSave->isChecked());
//    }
//    else
//    {
//        ui.checkBox_ImageSave->setChecked(!ui.checkBox_ImageSave->isChecked());
//        m_pLog->WarnMsg(QStringLiteral("用户设置图像存储失败."));
//        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("相机未采集.\n设置无效."));
//    }
    m_pImageStorage->SetStore(ui.checkBox_ImageSave->isChecked());
    if (ui.checkBox_ImageSave->isChecked())
    {
        m_pLog->WarnMsg(QStringLiteral("用户开启图像存储."));
    }
    else
    {
        m_pLog->WarnMsg(QStringLiteral("用户停止图像存储."));
    }
}

void UI_CtrlPad::on_pushButton_PlayPathBrowse_clicked(void)
{
    if (!m_pGrabber->GetGrabStatus())
    {
#if 1
        m_qstrPathReplay = m_qstrPathReplay == "" ? "/storage/DPS/TASK" : m_qstrPathReplay;
        QString qstrPath = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图像浏览路径"), m_qstrPathReplay);
//        QString qstrPath = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图像浏览路径"), m_qstrPathReplay, QFileDialog::DontUseNativeDialog);
        if (!qstrPath.isEmpty())
        {
            if (ui.checkBox_TrackProc->isChecked())
            {
                switch (ui.comboBox_TrackMode->currentIndex())
                {
                case 0: // "凝视搜索/引导跟踪（全帧）"
                    m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 1: // "恒动搜索（全帧）"
                    m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 2: // "闭环跟踪 （开窗）"
                    m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 3: // "星校（开窗）"
                    m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 4: // "手动提取（全帧）"
                    m_pImageProcessor->InitTracker(TRACK_MANUAL, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                    break;
                default:
                    break;
                }
            }
            else
            {
                m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(false);
                m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
            }
            m_qstrPathReplay = qstrPath;

            ui.tableWidget_TargetInfo->clear();
            QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
            ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
            ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));

            if (!m_pImageReplayer->SetReplayPath(qstrPath))
            {
                if (m_pGParam->m_qstrImageFormat == "raw")
                    QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
                else if (m_pGParam->m_qstrImageFormat == "bmp")
                    QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
            }
            ui.lineEdit_ImagePlayPath->setText(m_qstrPathReplay);
        }
        else
        {
            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像浏览路径无效.\n设置无效."));
        }
#else
        m_qstrPathReplay = m_qstrPathReplay == "" ? "/storage/DPS/TASK" : m_qstrPathReplay;
        QFileDialog *fileDialog = new QFileDialog(this, QStringLiteral("请选择图像浏览路径"), m_qstrPathReplay);
        fileDialog->setFileMode(QFileDialog::Directory);
        fileDialog->setOption(QFileDialog::ShowDirsOnly);
        fileDialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(fileDialog, &QFileDialog::finished, this, [=](int result) {
            if (result == QDialog::Accepted) {
                QString qstrPath = fileDialog->selectedFiles().isEmpty() ? "" : fileDialog->selectedFiles().first();
                if (!qstrPath.isEmpty())
                {
                    if (ui.checkBox_TrackProc->isChecked())
                    {
                        switch (ui.comboBox_TrackMode->currentIndex())
                        {
                        case 0: // "凝视搜索/引导跟踪（全帧）"
                            m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                            m_pImageProcessor->SetTrackProc(true);
                            break;
                        case 1: // "恒动搜索（全帧）"
                            m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                            m_pImageProcessor->SetTrackProc(true);
                            break;
                        case 2: // "闭环跟踪 （开窗）"
                            m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                            m_pImageProcessor->SetTrackProc(true);
                            break;
                        case 3: // "星校（开窗）"
                            m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                            m_pImageProcessor->SetTrackProc(true);
                            break;
                        case 4: // "手动提取（全帧）"
                            m_pImageProcessor->InitTracker(TRACK_MANUAL, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                            m_pImageProcessor->SetTrackProc(true);
                            m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                            break;
                        default:
                            break;
                        }
                    }
                    else
                    {
                        m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                        m_pImageProcessor->SetTrackProc(false);
                        m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                    }
                    m_qstrPathReplay = qstrPath;

                    ui.tableWidget_TargetInfo->clear();
                    QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
                    ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
                    ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));

                    if (!m_pImageReplayer->SetReplayPath(qstrPath))
                    {
                        if (m_pGParam->m_qstrImageFormat == "raw")
                            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
                        else if (m_pGParam->m_qstrImageFormat == "bmp")
                            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
                    }
                    ui.lineEdit_ImagePlayPath->setText(m_qstrPathReplay);
                }
                else
                {
                    QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像浏览路径无效.\n设置无效."));
                }
            }
            fileDialog->deleteLater(); // 删除对话框，释放内存
//            delete fileDialog;
        });
        fileDialog->open(); // 打开对话框
#endif
    }
}

void UI_CtrlPad::on_pushButton_Previous_clicked(void)
{
    if (!m_pGrabber->GetGrabStatus())
    {
        if (!m_pImageReplayer->SetBrowse(false))
        {
            if (m_pGParam->m_qstrImageFormat == "raw")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
            else if (m_pGParam->m_qstrImageFormat == "bmp")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
        }
    }
}

void UI_CtrlPad::on_pushButton_PreviousAuto_clicked(void)
{
    if (!m_pGrabber->GetGrabStatus())
    {
        if (!m_pImageReplayer->SetReplay(false))
        {
            if (m_pGParam->m_qstrImageFormat == "raw")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
            else if (m_pGParam->m_qstrImageFormat == "bmp")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
        }
    }
}

void UI_CtrlPad::on_pushButton_Pause_clicked(void)
{
    m_pImageReplayer->SetPause();
}

void UI_CtrlPad::on_pushButton_NextAuto_clicked(void)
{
    if (!m_pGrabber->GetGrabStatus())
    {
        if (!m_pImageReplayer->SetReplay(true))
        {
            if (m_pGParam->m_qstrImageFormat == "raw")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
            else if (m_pGParam->m_qstrImageFormat == "bmp")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
        }
    }
}

void UI_CtrlPad::on_pushButton_Next_clicked(void)
{
    if (!m_pGrabber->GetGrabStatus())
    {
        if (!m_pImageReplayer->SetBrowse(true))
        {
            if (m_pGParam->m_qstrImageFormat == "raw")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.raw)."));
            else if (m_pGParam->m_qstrImageFormat == "bmp")
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("浏览路径中无有效图像文件(.bmp)."));
        }
    }
}

void UI_CtrlPad::on_checkBox_EnhanceDisp_clicked()
{
    m_pImageProcessor->SetDispEnhance(ui.checkBox_EnhanceDisp->isChecked());
    emit SignalDispStatus();
}

void UI_CtrlPad::on_lineEdit_BlobDown_returnPressed(void)
{
    unsigned int uiBlobDown = ui.lineEdit_BlobDown->text().toInt();
    unsigned int uiBlobUp = ui.lineEdit_BlobUp->text().toInt();

    int iProcMode = m_pImageProcessor->GetProcMode();
    int iTrackMode = m_pImageProcessor->GetTrackMode();
    if ((iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
      || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC))
    {
        if (uiBlobDown < 20 || uiBlobDown >= uiBlobUp)
        {
            uiBlobDown = 20;
            uiBlobUp = 15000;
//            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
        }
    }
    else
    {
        if (uiBlobDown < 3 || uiBlobDown >= uiBlobUp)
        {
            uiBlobDown = 8;
            uiBlobUp = 2500;
//            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
        }
    }

    m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
}

void UI_CtrlPad::on_lineEdit_BlobUp_returnPressed(void)
{
    unsigned int uiBlobDown = ui.lineEdit_BlobDown->text().toInt();
    unsigned int uiBlobUp = ui.lineEdit_BlobUp->text().toInt();

    int iProcMode = m_pImageProcessor->GetProcMode();
    int iTrackMode = m_pImageProcessor->GetTrackMode();
    if ((iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
      || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC))
    {
        if (uiBlobDown < 20 || uiBlobDown >= uiBlobUp)
        {
            uiBlobDown = 20;
            uiBlobUp = 15000;
//            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
        }
    }
    else
    {
        if (uiBlobDown < 3 || uiBlobDown >= uiBlobUp)
        {
            uiBlobDown = 8;
            uiBlobUp = 2500;
//            QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
        }
    }

    m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
}

void UI_CtrlPad::on_comboBox_TrackMode_activated(int iIndex)
{
    LoadParams();
    WriteParams();
    m_pImageProcessor->Init_TWDW();

    ui.tableWidget_TargetInfo->clear();
    QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
    ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
    ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
}

void UI_CtrlPad::on_pushButton_TrackParamsRefresh_clicked()
{
    LoadParams();
}

void UI_CtrlPad::on_pushButton_TrackParamsSet_clicked()
{
    WriteParams();
}

void UI_CtrlPad::on_checkBox_TrackSave_clicked(void)
{
    m_pTrackDataStorage->SetStore(ui.checkBox_TrackSave->isChecked());
    if (ui.checkBox_TrackSave->isChecked())
    {
        QDateTime qdatetimeCurrent = QDateTime::currentDateTime();
        QString qstrCurrentTime1 = qdatetimeCurrent.toString("yyyyMMddhhmmss");
        QString qstrCurrentTime2 = qdatetimeCurrent.toString("yyyyMM");
        QString qstrCurrentTime3 = qdatetimeCurrent.toString("yyyyMMdd");
        QString qstrCurrentTime4 = qdatetimeCurrent.toString("yyyy");
        QString qstrID = m_pGParam->m_SNetMasterControlData.qstrTargetID;
        QString qstrFileName = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrCurrentTime4 + "/" + qstrCurrentTime2 + "/" + qstrCurrentTime3 + "/" + qstrID + "/track_" + qstrID + "_" + qstrCurrentTime1 + ".trk";
        QString qstrPath = qstrFileName.left(qstrFileName.lastIndexOf('/'));
        QDir dir;
        if (!dir.exists(qstrPath))
        {
            dir.mkpath(qstrPath);
        }
        m_pTrackDataStorage->SetFileName(qstrFileName);
        m_pLog->InfoMsg(QStringLiteral("用户设置跟踪数据存储."));
    }
}

void UI_CtrlPad::on_checkBox_TrackInfoEN_clicked()
{
    if (ui.checkBox_TrackInfoEN->isChecked())
    {
        ui.textEdit_TrackInfo->setReadOnly(false);
        ui.textEdit_TrackInfo->setFrameShape(QFrame::NoFrame);
    }
    else
    {
        ui.textEdit_TrackInfo->setReadOnly(true);
        ui.textEdit_TrackInfo->setFrameShape(QFrame::StyledPanel);
    }
}

void UI_CtrlPad::on_checkBox_TrackProc_clicked(bool bChecked)
{
    if (ui.checkBox_MCCtrl->isChecked())
    {
        m_pLog->InfoMsg(QStringLiteral("用户干预搜索或跟踪."));
        if (bChecked)
        {
            switch (ui.comboBox_TrackMode->currentIndex())
            {
            case 0: // "凝视搜索/引导跟踪（全帧）"
                m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                break;
            case 1: // "恒动搜索（全帧）"
                m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());                break;
            case 2: // "闭环跟踪 （开窗）"
                m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                break;
            case 3: // "星校（开窗）"
                m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                break;
            case 4: // "手动提取（全帧）"
                m_pImageProcessor->InitTracker(TRACK_MANUAL, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                break;
            default:
                break;
            }
            m_pImageProcessor->SetTrackProc(true);
        }
        else
        {
            m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            m_pImageProcessor->SetTrackProc(false);
            m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
        }
        ui.tableWidget_TargetInfo->clear();
        QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
        ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
        ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
    }
    else
    {
        if (ui.checkBox_TrackProc->isChecked())
        {
            switch (ui.comboBox_TrackMode->currentIndex())
            {
            case 0: // "凝视搜索/引导跟踪（全帧）"
                m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(true);
                break;
            case 1: // "恒动搜索（全帧）"
                m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(true);
                break;
            case 2: // "闭环跟踪 （开窗）"
                m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(true);
                if (m_pGParam->m_SGrabberData.dExposureTime != 45)
                {
                    LoadParams();
                    WriteParams();
                }
                break;
            case 3: // "星校（开窗）"
                m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(true);
                if (m_pGParam->m_SGrabberData.dExposureTime != 45)
                {
                    LoadParams();
                    WriteParams();
                }
                break;
            case 4: // "手动提取（全帧）"
                m_pImageProcessor->InitTracker(TRACK_MANUAL, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(true);
                m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                break;
            default:
                break;
            }
            m_pLog->InfoMsg(QStringLiteral("用户开启搜索或跟踪."));
        }
        else
        {
            m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            m_pImageProcessor->SetTrackProc(false);
            m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
            m_pLog->InfoMsg(QStringLiteral("用户停止搜索或跟踪."));
        }
        ui.tableWidget_TargetInfo->clear();
        QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
        ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
        ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
    }
}

void UI_CtrlPad::on_pushButton_DistCurve_clicked()
{
    m_UI_DistCurve = new UI_DistCurve;

    QFileDialog* fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle("Select a track file");
    fileDialog->setDirectory(m_pGParam->m_SPath.qstrDataStorePath);
    fileDialog->setNameFilter(tr("Track Files (*.trk)"));
    fileDialog->setViewMode(QFileDialog::Detail);

    if(fileDialog->exec())
    {
        QStringList fileNames = fileDialog->selectedFiles();

        //![1]
        QLineSeries *seriesDx = new QLineSeries();
        QLineSeries *seriesDy = new QLineSeries();

        QFile sunSpots(fileNames[0]);
        if (!sunSpots.open(QIODevice::ReadOnly | QIODevice::Text)) {
    //        return 1;
        }

        long long llBegin = -1;
        QTextStream stream(&sunSpots);
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.startsWith("#") || line.startsWith(":"))
                continue;
            QStringList values = line.split(" ", QString::SkipEmptyParts);
            if (llBegin == -1)  llBegin = values[0].toInt();
            seriesDx->append(values[0].toInt() - llBegin, values[5].toDouble());
            seriesDy->append(values[0].toInt() - llBegin, values[6].toDouble());
        }
        sunSpots.close();
        //![2]

        //![3]
        QChart *chart = new QChart();
        chart->addSeries(seriesDx);
        chart->addSeries(seriesDy);
//        chart->legend()->hide();
        chart->setTitle("Dx & Dy (Pixels)");
        //![3]

        //![4]
        QValueAxis *axisX = new QValueAxis;
        axisX->setTickCount(10);
        axisX->setTitleText("Sequence of Images");
        axisX->setLabelFormat("%d");
        chart->addAxis(axisX, Qt::AlignBottom);
        seriesDx->attachAxis(axisX);
        seriesDx->setName("Dx");

        QValueAxis *axisY = new QValueAxis;
        axisY->setTitleText("Pixels");
        chart->addAxis(axisY, Qt::AlignLeft);
        seriesDy->attachAxis(axisY);
        seriesDy->setName("Dy");
        //![4]

        //![5]
        QChartView *chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);
        //![5]
        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(chartView);

        //![6]
        m_UI_DistCurve->setLayout(layout);
        m_UI_DistCurve->resize(820, 600);
        m_UI_DistCurve->show();
    }
}

void UI_CtrlPad::on_pushButton_DistCurve_2_clicked()
{
    m_UI_DistCurve = new UI_DistCurve;

    QFileDialog* fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle("Select a GDJ file");
    fileDialog->setDirectory(m_pGParam->m_SPath.qstrDataStorePath);
    fileDialog->setNameFilter(tr("GDJ Files (*.GDJ)"));
    fileDialog->setViewMode(QFileDialog::Detail);

    if(fileDialog->exec())
    {
        QStringList fileNames = fileDialog->selectedFiles();
        std::string strFileName = fileNames[0].toLocal8Bit().data();

        GetPeriod* gp = new GetPeriod;

        std::vector<double> vectdTime;
        std::vector<double> vectdMag;
        gp->LoadGDJFile(vectdTime, vectdMag, strFileName);
        float* period = NULL;
        int period_size=0;
        float f_base;
        gp->getperiod(&vectdTime[0], &vectdMag[0], vectdTime.size(), period, period_size, f_base);

        bool bPeriod = period != NULL;
        float tp = 0;
        QString qstrRes;
        if (bPeriod)
        {
            if (*period > 1e-8)
            {
                tp = 1.0 / (*period * f_base);
                delete gp;
                qstrRes.sprintf("Period: %.3f s", tp * 24 * 3600.0);
            }
            else
            {
                qstrRes = "Invalid period.";
            }
        }
        else
        {
            qstrRes = "Invalid period.";
        }
        qDebug() << qstrRes;

        QChart *chart = new QChart();
        QLineSeries *seriesDN = new QLineSeries();
        DateTime dtTime;
        for (int i = 0; i < vectdMag.size(); i++)
        {
            dtTime = dtTime.MJD2date(vectdTime[i]-8.0/24.0);
            seriesDN->append(dtTime.hour * 3600 + dtTime.minute*60 + dtTime.second + dtTime.millisecond/1000.0, vectdMag[i]);
        }

        chart->addSeries(seriesDN);
//        chart->legend()->hide();
        chart->setTitle(qstrRes);

        QValueAxis *axisX = new QValueAxis;
        axisX->setTitleText("UTC Time / s");
        axisX->setLabelFormat("%.1f");
        chart->setAxisX(axisX, seriesDN);
        axisX->setGridLineVisible(true);
        axisX->setGridLineColor(QColor(0, 0, 0));
        axisX->setTickCount(10);
        QValueAxis *axisY = new QValueAxis;
        axisY->setTitleText("Photometry / mV");
        axisY->setLabelFormat("%.1f");
        chart->setAxisY(axisY, seriesDN);
        axisY->setGridLineVisible(true);
        axisY->setGridLineColor(QColor(0, 0, 0));
        axisY->setTickCount(0.1);

        QChartView *chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);
        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(chartView);

        m_UI_DistCurve->setLayout(layout);
        m_UI_DistCurve->resize(1500, 900);
        m_UI_DistCurve->show();
    }
}

void UI_CtrlPad::LoadParams()
{
    m_bParamsLoading = true;
    ui.checkBox_MCCtrl->setEnabled(false);
    ui.checkBox_TrackProc->setEnabled(false);
    ui.checkBox_TrackSave->setEnabled(false);
    ui.checkBox_DrawStarMap->setEnabled(false);
    ui.checkBox_DistValid->setEnabled(false);
    ui.checkBox_SpdValid->setEnabled(false);
    ui.comboBox_TrackMode_2->setEnabled(false);
    ui.checkBox_AutoScale->setEnabled(false);
    ui.checkBox_EnhanceDisp->setEnabled(false);
    ui.checkBox_TrackInfoEN->setEnabled(false);
    ui.pushButton_TrackParamsRefresh->setEnabled(false);
    ui.pushButton_TrackParamsSet->setEnabled(false);
    ui.lineEdit_BlobUp->setEnabled(false);
    ui.lineEdit_BlobDown->setEnabled(false);

    bool bEnabled = ui.checkBox_ImageGrab->isEnabled();
    if (bEnabled) ui.checkBox_ImageGrab->setEnabled(false);
    bool bGrabbing = m_pGrabber->GetGrabStatus();
    if(bGrabbing) m_pGrabber->PauseGrab();
    m_pImageReplayer->SetPause();

    int iProcMode, iTrackMode;
    if (!ui.checkBox_MCCtrl->isChecked())    ui.comboBox_TrackMode->setEnabled(false);
    double dOptCenterX, dOptCenterY, dPixelScale;
    QString qstrLine;
    QSettings* qsetSystemInit = NULL;
    sTrackingSettings settings;
    unsigned int uiBlobDown, uiBlobUp;
    switch (ui.comboBox_TrackMode->currentIndex())
    {
    case 0: // "凝视搜索/引导跟踪（全帧）"
        iProcMode = PROC_DIRECT;
        iTrackMode = TRACK_GEO;
        /// 配置文件读取
        qsetSystemInit = new QSettings("/home/dps/IniFiles/Proc_NoCloseLoop.ini", QSettings::IniFormat);
        if (qsetSystemInit)
        {
            /// [OpticSettings]
            qsetSystemInit->beginGroup("OpticSettings");
            if (qsetSystemInit->contains("OptCenterX")
                && qsetSystemInit->contains("OptCenterY")
                && qsetSystemInit->contains("PixelScale"))
            {
                dOptCenterX = qsetSystemInit->value("OptCenterX").toDouble();
                dOptCenterY = qsetSystemInit->value("OptCenterY").toDouble();
                dPixelScale = qsetSystemInit->value("PixelScale").toDouble();
            }
            settings.opticparams.fFOVCenterX = dOptCenterX;
            settings.opticparams.fFOVCenterY = dOptCenterY;
            settings.opticparams.fPixelScale = dPixelScale;
            qsetSystemInit->endGroup();
            /// [CameraSettings]
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("GrabBinning")
                && qsetSystemInit->contains("GrabWidth")
                && qsetSystemInit->contains("GrabHeight")
                && qsetSystemInit->contains("ExposureTime")
                && qsetSystemInit->contains("Gain")
                && qsetSystemInit->contains("Trigger")
                && qsetSystemInit->contains("FrameFrequency"))
            {
                m_pGParam->m_SGrabberData.bSub = false;
                m_pGParam->m_SGrabberData.iFullBinning = qsetSystemInit->value("GrabBinning").toInt();
                m_pGParam->m_SGrabberData.iFullWidth = qsetSystemInit->value("GrabWidth").toInt();
                m_pGParam->m_SGrabberData.iFullHeight = qsetSystemInit->value("GrabHeight").toInt();
                m_pGParam->m_SGrabberData.dExposureTime = qsetSystemInit->value("ExposureTime").toDouble() >= 1.3 ? qsetSystemInit->value("ExposureTime").toDouble() : 1.3;
//                m_pGParam->m_SGrabberData.dExposureTime = (ui.checkBox_TrackProc->isChecked() && m_pGParam->m_SGrabberData.dExposureTime < 1500) ? 1500 : m_pGParam->m_SGrabberData.dExposureTime;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() >= 1.0 ? qsetSystemInit->value("Gain").toDouble() : 1.0;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() <= 10.0 ? qsetSystemInit->value("Gain").toDouble() : 10.0;
                m_pGParam->m_SGrabberData.iTriggerMode = qsetSystemInit->value("Trigger").toInt();
                m_pGParam->m_SGrabberData.dFrameFrequency = qsetSystemInit->value("FrameFrequency").toDouble();
            }
            qsetSystemInit->endGroup();
            /// [TrackSettings]
            qsetSystemInit->beginGroup("TrackSettings");
            if (qsetSystemInit->contains("ThreashLiving")
                && qsetSystemInit->contains("NumFramesLiving")
                && qsetSystemInit->contains("Radius")
                && qsetSystemInit->contains("RatioFOV")
                && qsetSystemInit->contains("ThreashStarMode")
                && qsetSystemInit->contains("ThreashGazeMode")
                && qsetSystemInit->contains("AutoDecide")
                && qsetSystemInit->contains("ThreashMEO")
                && qsetSystemInit->contains("SpdLow_AE")
                && qsetSystemInit->contains("SpdHigh_AE"))
            {
                settings.fThreashLiving = qsetSystemInit->value("ThreashLiving").toFloat();
                settings.iNumFramesLiving = qsetSystemInit->value("NumFramesLiving").toInt();
                settings.fRadius = qsetSystemInit->value("Radius").toFloat();
                settings.fRatioFOV = qsetSystemInit->value("RatioFOV").toFloat();
                settings.fThreashStarMode = qsetSystemInit->value("ThreashStarMode").toFloat();
                settings.fThreashGazeMode = qsetSystemInit->value("ThreashGazeMode").toFloat();
                settings.bAutoDecide = qsetSystemInit->value("AutoDecide").toBool();
                settings.fThreashMEO = qsetSystemInit->value("ThreashMEO").toFloat();
                settings.fSpdLow_AE = qsetSystemInit->value("SpdLow_AE").toFloat();
                settings.fSpdHigh_AE = qsetSystemInit->value("SpdHigh_AE").toFloat();
                settings.fThreash_AE = qsetSystemInit->value("Threash_AE").toFloat();
            }
            qsetSystemInit->endGroup();
            /// 相机控制
            m_pGParam->m_SGrabberData.bWindowEN = false;            
            if (m_pGParam->m_SGrabberData.iWorkingStatus == STATUS_OK)
            {
                if (m_uiIndexTrackMode == 2
                        || m_uiIndexTrackMode == 3
                        || m_uiIndexTrackMode == 10000000)
                {
                    QThread::msleep(5);
                    m_pGrabber->SetWindowEN(m_pGParam->m_SGrabberData.iFullHeight, 0);
                }
                m_uiIndexTrackMode = ui.comboBox_TrackMode->currentIndex();
                if (m_fExpTime != m_pGParam->m_SGrabberData.dExposureTime)
                {
                    m_fExpTime = m_pGParam->m_SGrabberData.dExposureTime;
                    QThread::msleep(5);
                    QString qstrExpTime = QString::number(m_pGParam->m_SGrabberData.dExposureTime, 'f', 3);
                    m_pGrabber->SetExpTime(qstrExpTime);

                }
                if (m_fGain != m_pGParam->m_SGrabberData.dGain)
                {
                    m_fGain = m_pGParam->m_SGrabberData.dGain;
                    QThread::msleep(5);
                    QString qstrGain = QString::number(m_pGParam->m_SGrabberData.dGain, 'f', 3);
                    m_pGrabber->SetGain(qstrGain);
                }
                m_pGParam->m_SCommExposureData.iTriggerModeSend = TRIGGERMODE_FREE;
                m_pGParam->m_SCommExposureData.dFrameFrequencySend = 1;
            }
            /// 图像处理控制
            m_pImageProcessor->InitProc(PROC_DIRECT, TRACK_GEO,
                                        m_pGParam->m_SGrabberData.iFullWidth,
                                        m_pGParam->m_SGrabberData.iFullHeight,
                                        1, 1024, 1024, m_pImageProcessor->GetProcessMode(), settings);
            uiBlobDown = (m_fExpTime <= 100 && m_fExpTime != 0) ? 20 : 8;
            uiBlobUp = (m_fExpTime <= 100 && m_fExpTime != 0) ? 15000 : 2500;
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
            m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
            /// 控件显示
            ui.textEdit_TrackInfo->clear();
            ui.textEdit_TrackInfo->append("[OpticSettings]");
            qstrLine.sprintf("OptCenterX = %.3f", dOptCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("OptCenterY = %.3f", dOptCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("PixelScale = %.9f", dPixelScale);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[CameraSettings]");
            qstrLine.sprintf("GrabBinning = %d", m_pGParam->m_SGrabberData.iFullBinning);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabWidth = %d", m_pGParam->m_SGrabberData.iFullWidth);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabHeight = %d", m_pGParam->m_SGrabberData.iFullHeight);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ExposureTime = %.3f", m_pGParam->m_SGrabberData.dExposureTime);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Gain = %.3f", m_pGParam->m_SGrabberData.dGain);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Trigger = %d", m_pGParam->m_SGrabberData.iTriggerMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("FrameFrequency = %d", m_pGParam->m_SGrabberData.dFrameFrequency);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[TrackSettings]");
            qstrLine.sprintf("ThreashLiving = %.3f", settings.fThreashLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("NumFramesLiving = %d", settings.iNumFramesLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Radius = %.3f", settings.fRadius);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("RatioFOV = %.3f", settings.fRatioFOV);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashStarMode = %.3f", settings.fThreashStarMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashGazeMode = %.3f", settings.fThreashGazeMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("AutoDecide = %d", settings.bAutoDecide ? 1 : 0);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashMEO = %.3f", settings.fThreashMEO);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdLow_AE = %.3f", settings.fSpdLow_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdHigh_AE = %.3f", settings.fSpdHigh_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Threash_AE = %.3f", settings.fThreash_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            m_pLog->InfoMsg(QStringLiteral("用户设置'凝视搜索/引导跟踪（全帧）'踪模式."));
        }
        break;
    case 1: // "恒动搜索（全帧）"
        iProcMode = PROC_DIFF;
        iTrackMode = TRACK_GEO;
        /// 配置文件读取
        qsetSystemInit = new QSettings("/home/dps/IniFiles/Proc_Search.ini", QSettings::IniFormat);
        if (qsetSystemInit)
        {
            /// [OpticSettings]
            qsetSystemInit->beginGroup("OpticSettings");
            if (qsetSystemInit->contains("OptCenterX")
                && qsetSystemInit->contains("OptCenterY")
                && qsetSystemInit->contains("PixelScale"))
            {
                dOptCenterX = qsetSystemInit->value("OptCenterX").toDouble();
                dOptCenterY = qsetSystemInit->value("OptCenterY").toDouble();
                dPixelScale = qsetSystemInit->value("PixelScale").toDouble();
            }
            settings.opticparams.fFOVCenterX = dOptCenterX;
            settings.opticparams.fFOVCenterY = dOptCenterY;
            settings.opticparams.fPixelScale = dPixelScale;
            qsetSystemInit->endGroup();
            /// [CameraSettings]
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("GrabBinning")
                && qsetSystemInit->contains("GrabWidth")
                && qsetSystemInit->contains("GrabHeight")
                && qsetSystemInit->contains("ExposureTime")
                && qsetSystemInit->contains("Gain")
                && qsetSystemInit->contains("Trigger")
                && qsetSystemInit->contains("FrameFrequency"))
            {
                m_pGParam->m_SGrabberData.bSub = false;
                m_pGParam->m_SGrabberData.iFullBinning = qsetSystemInit->value("GrabBinning").toInt();
                m_pGParam->m_SGrabberData.iFullWidth = qsetSystemInit->value("GrabWidth").toInt();
                m_pGParam->m_SGrabberData.iFullHeight = qsetSystemInit->value("GrabHeight").toInt();
                m_pGParam->m_SGrabberData.dExposureTime = qsetSystemInit->value("ExposureTime").toDouble() >= 1.3 ? qsetSystemInit->value("ExposureTime").toDouble() : 1.3;
//                m_pGParam->m_SGrabberData.dExposureTime = (ui.checkBox_TrackProc->isChecked() && m_pGParam->m_SGrabberData.dExposureTime < 1500) ? 1500 : m_pGParam->m_SGrabberData.dExposureTime;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() >= 1.0 ? qsetSystemInit->value("Gain").toDouble() : 1.0;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() <= 10.0 ? qsetSystemInit->value("Gain").toDouble() : 10.0;
                m_pGParam->m_SGrabberData.iTriggerMode = qsetSystemInit->value("Trigger").toInt();
                m_pGParam->m_SGrabberData.dFrameFrequency = qsetSystemInit->value("FrameFrequency").toDouble();
            }
            qsetSystemInit->endGroup();
            /// [TrackSettings]
            qsetSystemInit->beginGroup("TrackSettings");
            if (qsetSystemInit->contains("ThreashLiving")
                && qsetSystemInit->contains("NumFramesLiving")
                && qsetSystemInit->contains("Radius")
                && qsetSystemInit->contains("RatioFOV")
                && qsetSystemInit->contains("ThreashStarMode")
                && qsetSystemInit->contains("ThreashGazeMode")
                && qsetSystemInit->contains("AutoDecide")
                && qsetSystemInit->contains("ThreashMEO")
                && qsetSystemInit->contains("SpdLow_AE")
                && qsetSystemInit->contains("SpdHigh_AE"))
            {
                settings.fThreashLiving = qsetSystemInit->value("ThreashLiving").toFloat();
                settings.iNumFramesLiving = qsetSystemInit->value("NumFramesLiving").toInt();
                settings.fRadius = qsetSystemInit->value("Radius").toFloat();
                settings.fRatioFOV = qsetSystemInit->value("RatioFOV").toFloat();
                settings.fThreashStarMode = qsetSystemInit->value("ThreashStarMode").toFloat();
                settings.fThreashGazeMode = qsetSystemInit->value("ThreashGazeMode").toFloat();
                settings.bAutoDecide = qsetSystemInit->value("AutoDecide").toBool();
                settings.fThreashMEO = qsetSystemInit->value("ThreashMEO").toFloat();
                settings.fSpdLow_AE = qsetSystemInit->value("SpdLow_AE").toFloat();
                settings.fSpdHigh_AE = qsetSystemInit->value("SpdHigh_AE").toFloat();
                settings.fThreash_AE = qsetSystemInit->value("Threash_AE").toFloat();
            }
            qsetSystemInit->endGroup();
            /// 相机控制
            m_pGParam->m_SGrabberData.bWindowEN = false;
            if (m_pGParam->m_SGrabberData.iWorkingStatus == STATUS_OK)
            {      
                if (m_uiIndexTrackMode == 2
                        || m_uiIndexTrackMode == 3
                        || m_uiIndexTrackMode == 10000000)
                {
                    QThread::msleep(5);
                    m_pGrabber->SetWindowEN(m_pGParam->m_SGrabberData.iFullHeight, 0);
                }
                m_uiIndexTrackMode = ui.comboBox_TrackMode->currentIndex();
                if (m_fExpTime != m_pGParam->m_SGrabberData.dExposureTime)
                {
                    m_fExpTime = m_pGParam->m_SGrabberData.dExposureTime;
                    QThread::msleep(5);
                    QString qstrExpTime = QString::number(m_pGParam->m_SGrabberData.dExposureTime, 'f', 3);
                    m_pGrabber->SetExpTime(qstrExpTime);
                }
                if (m_fGain != m_pGParam->m_SGrabberData.dGain)
                {
                    QThread::msleep(5);
                    QString qstrGain = QString::number(m_pGParam->m_SGrabberData.dGain, 'f', 3);
                    m_pGrabber->SetGain(qstrGain);
                }
                m_pGParam->m_SCommExposureData.iTriggerModeSend = TRIGGERMODE_FREE;
                m_pGParam->m_SCommExposureData.dFrameFrequencySend = 1;
            }
            /// 图像处理控制
            m_pImageProcessor->InitProc(PROC_DIFF, TRACK_GEO,
                                        m_pGParam->m_SGrabberData.iFullWidth,
                                        m_pGParam->m_SGrabberData.iFullHeight,
                                        1, 1024, 1024, m_pImageProcessor->GetProcessMode(), settings);
            uiBlobDown = 20;
            uiBlobUp = 15000;
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
            m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
            /// 控件显示
            ui.textEdit_TrackInfo->clear();
            ui.textEdit_TrackInfo->append("[OpticSettings]");
            qstrLine.sprintf("OptCenterX = %.3f", dOptCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("OptCenterY = %.3f", dOptCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("PixelScale = %.9f", dPixelScale);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[CameraSettings]");
            qstrLine.sprintf("GrabBinning = %d", m_pGParam->m_SGrabberData.iFullBinning);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabWidth = %d", m_pGParam->m_SGrabberData.iFullWidth);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabHeight = %d", m_pGParam->m_SGrabberData.iFullHeight);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ExposureTime = %.3f", m_pGParam->m_SGrabberData.dExposureTime);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Gain = %.3f", m_pGParam->m_SGrabberData.dGain);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Trigger = %d", m_pGParam->m_SGrabberData.iTriggerMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("FrameFrequency = %d", m_pGParam->m_SGrabberData.dFrameFrequency);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[TrackSettings]");
            qstrLine.sprintf("ThreashLiving = %.3f", settings.fThreashLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("NumFramesLiving = %d", settings.iNumFramesLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Radius = %.3f", settings.fRadius);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("RatioFOV = %.3f", settings.fRatioFOV);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashStarMode = %.3f", settings.fThreashStarMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashGazeMode = %.3f", settings.fThreashGazeMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("AutoDecide = %d", settings.bAutoDecide ? 1 : 0);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashMEO = %.3f", settings.fThreashMEO);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdLow_AE = %.3f", settings.fSpdLow_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdHigh_AE = %.3f", settings.fSpdHigh_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Threash_AE = %.3f", settings.fThreash_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            m_pLog->InfoMsg(QStringLiteral("用户设置'恒动搜索（全帧）'模式."));
        }
        break;
    case 2: // "闭环跟踪 （开窗）"
        iProcMode = PROC_DIRECT;
        iTrackMode = TRACK_LEO;
        /// 配置文件读取
        qsetSystemInit = new QSettings("/home/dps/IniFiles/Proc_CloseLoop.ini", QSettings::IniFormat);
        if (qsetSystemInit)
        {
            /// [OpticSettings]
            qsetSystemInit->beginGroup("OpticSettings");
            if (qsetSystemInit->contains("OptCenterX")
                && qsetSystemInit->contains("OptCenterY")
                && qsetSystemInit->contains("PixelScale"))
            {
                dOptCenterX = qsetSystemInit->value("OptCenterX").toDouble();
                dOptCenterY = qsetSystemInit->value("OptCenterY").toDouble();
                dPixelScale = qsetSystemInit->value("PixelScale").toDouble();
            }
            settings.opticparams.fFOVCenterX = dOptCenterX;
            settings.opticparams.fFOVCenterY = dOptCenterY;
            settings.opticparams.fPixelScale = dPixelScale;
            qsetSystemInit->endGroup();
            /// [CameraSettings]
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("GrabBinning")
                && qsetSystemInit->contains("SubInFullCenterX")
                && qsetSystemInit->contains("SubInFullCenterY")
                && qsetSystemInit->contains("GrabWidth")
                && qsetSystemInit->contains("GrabHeight")
                && qsetSystemInit->contains("ExposureTime")
                && qsetSystemInit->contains("Gain")
                && qsetSystemInit->contains("Trigger")
                && qsetSystemInit->contains("FrameFrequency"))
            {
                m_pGParam->m_SGrabberData.bSub = true;
                m_pGParam->m_SGrabberData.iFullBinning = qsetSystemInit->value("GrabBinning").toInt();
                m_pGParam->m_SGrabberData.iSubInFullCenterX = qsetSystemInit->value("SubInFullCenterX").toInt();
                m_pGParam->m_SGrabberData.iSubInFullCenterY= qsetSystemInit->value("SubInFullCenterY").toInt();
                m_pGParam->m_SGrabberData.iSubWidth = qsetSystemInit->value("GrabWidth").toInt();
                m_pGParam->m_SGrabberData.iSubHeight = qsetSystemInit->value("GrabHeight").toInt();
                m_pGParam->m_SGrabberData.dExposureTime = qsetSystemInit->value("ExposureTime").toDouble() >= 1.3 ? qsetSystemInit->value("ExposureTime").toDouble() : 1.3;
                m_pGParam->m_SGrabberData.dExposureTime = ui.checkBox_TrackProc->isChecked() ? 45 : m_pGParam->m_SGrabberData.dExposureTime;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() >= 1.0 ? qsetSystemInit->value("Gain").toDouble() : 1.0;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() <= 10.0 ? qsetSystemInit->value("Gain").toDouble() : 10.0;
                m_pGParam->m_SGrabberData.iTriggerMode = qsetSystemInit->value("Trigger").toInt();
                m_pGParam->m_SGrabberData.dFrameFrequency = qsetSystemInit->value("FrameFrequency").toDouble();
            }
            qsetSystemInit->endGroup();
            /// [TrackSettings]
            qsetSystemInit->beginGroup("TrackSettings");
            if (qsetSystemInit->contains("ThreashLiving")
                && qsetSystemInit->contains("NumFramesLiving")
                && qsetSystemInit->contains("Radius")
                && qsetSystemInit->contains("RatioFOV")
                && qsetSystemInit->contains("ThreashStarMode")
                && qsetSystemInit->contains("ThreashGazeMode")
                && qsetSystemInit->contains("AutoDecide")
                && qsetSystemInit->contains("ThreashMEO")
                && qsetSystemInit->contains("SpdLow_AE")
                && qsetSystemInit->contains("SpdHigh_AE")
                && qsetSystemInit->contains("Threash_AE"))
            {
                settings.fThreashLiving = qsetSystemInit->value("ThreashLiving").toFloat();
                settings.iNumFramesLiving = qsetSystemInit->value("NumFramesLiving").toInt();
                settings.fRadius = qsetSystemInit->value("Radius").toFloat();
                settings.fRatioFOV = qsetSystemInit->value("RatioFOV").toFloat();
                settings.fThreashStarMode = qsetSystemInit->value("ThreashStarMode").toFloat();
                settings.fThreashGazeMode = qsetSystemInit->value("ThreashGazeMode").toFloat();
                settings.bAutoDecide = qsetSystemInit->value("AutoDecide").toBool();
                settings.fThreashMEO = qsetSystemInit->value("ThreashMEO").toFloat();
                settings.fSpdLow_AE = qsetSystemInit->value("SpdLow_AE").toFloat();
                settings.fSpdHigh_AE = qsetSystemInit->value("SpdHigh_AE").toFloat();
                settings.fThreash_AE = qsetSystemInit->value("Threash_AE").toFloat();
            }
            qsetSystemInit->endGroup();
            /// 相机控制
            m_pGParam->m_SGrabberData.bWindowEN = true;
            if (m_pGParam->m_SGrabberData.iWorkingStatus == STATUS_OK)
            {
                if (m_uiIndexTrackMode == 0
                        || m_uiIndexTrackMode == 1)
                {
                    QThread::msleep(5);
                    m_pGrabber->SetWindowEN(m_pGParam->m_SGrabberData.iSubHeight, m_pGParam->m_SGrabberData.iSubInFullCenterY - m_pGParam->m_SGrabberData.iSubHeight / 2);
                }
                m_uiIndexTrackMode = ui.comboBox_TrackMode->currentIndex();
                if (m_fExpTime != m_pGParam->m_SGrabberData.dExposureTime)
                {
                    m_fExpTime = m_pGParam->m_SGrabberData.dExposureTime;
                    QThread::msleep(5);
                    QString qstrExpTime = QString::number(m_pGParam->m_SGrabberData.dExposureTime, 'f', 3);
                    m_pGrabber->SetExpTime(qstrExpTime);
                }
                if (m_fGain != m_pGParam->m_SGrabberData.dGain)
                {
                    QThread::msleep(5);
                    QString qstrGain = QString::number(m_pGParam->m_SGrabberData.dGain, 'f', 3);
                    m_pGrabber->SetGain(qstrGain);
                }
                m_pGParam->m_SCommExposureData.iTriggerModeSend = TRIGGERMODE_OUT;
                m_pGParam->m_SCommExposureData.dFrameFrequencySend = m_pGParam->m_SGrabberData.dFrameFrequency;
            }
            /// 图像处理控制
            m_pImageProcessor->InitProc(PROC_DIRECT, TRACK_LEO,
                                        m_pGParam->m_SGrabberData.iSubWidth,
                                        m_pGParam->m_SGrabberData.iSubHeight,
                                        1, 1024, 1024, m_pImageProcessor->GetProcessMode(), settings);
            uiBlobDown = 20;
            uiBlobUp = 15000;
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
            m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
            /// 控件显示
            ui.textEdit_TrackInfo->clear();
            ui.textEdit_TrackInfo->append("[OpticSettings]");
            qstrLine.sprintf("OptCenterX = %.3f", dOptCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("OptCenterY = %.3f", dOptCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("PixelScale = %.9f", dPixelScale);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[CameraSettings]");
            qstrLine.sprintf("GrabBinning = %d", m_pGParam->m_SGrabberData.iFullBinning);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SubInFullCenterX = %d", m_pGParam->m_SGrabberData.iSubInFullCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SubInFullCenterY = %d", m_pGParam->m_SGrabberData.iSubInFullCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabWidth = %d", m_pGParam->m_SGrabberData.iSubWidth);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabHeight = %d", m_pGParam->m_SGrabberData.iSubHeight);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ExposureTime = %.3f", m_pGParam->m_SGrabberData.dExposureTime);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Gain = %.3f", m_pGParam->m_SGrabberData.dGain);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Trigger = %d", m_pGParam->m_SGrabberData.iTriggerMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("FrameFrequency = %d", m_pGParam->m_SGrabberData.dFrameFrequency);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[TrackSettings]");
            qstrLine.sprintf("ThreashLiving = %.3f", settings.fThreashLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("NumFramesLiving = %d", settings.iNumFramesLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Radius = %.3f", settings.fRadius);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("RatioFOV = %.3f", settings.fRatioFOV);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashStarMode = %.3f", settings.fThreashStarMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashGazeMode = %.3f", settings.fThreashGazeMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("AutoDecide = %d", settings.bAutoDecide ? 1 : 0);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashMEO = %.3f", settings.fThreashMEO);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdLow_AE = %.3f", settings.fSpdLow_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdHigh_AE = %.3f", settings.fSpdHigh_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Threash_AE = %.3f", settings.fThreash_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            m_pLog->InfoMsg(QStringLiteral("用户设置'闭环跟踪 （开窗）'模式."));
        }
        break;
    case 3: // "星校（开窗）"
        iProcMode = PROC_DIRECT;
        iTrackMode = TRACK_SC;
        /// 配置文件读取
        qsetSystemInit = new QSettings("/home/dps/IniFiles/Proc_StarCalib.ini", QSettings::IniFormat);
        if (qsetSystemInit)
        {
            /// [OpticSettings]
            qsetSystemInit->beginGroup("OpticSettings");
            if (qsetSystemInit->contains("OptCenterX")
                && qsetSystemInit->contains("OptCenterY")
                && qsetSystemInit->contains("PixelScale"))
            {
                dOptCenterX = qsetSystemInit->value("OptCenterX").toDouble();
                dOptCenterY = qsetSystemInit->value("OptCenterY").toDouble();
                dPixelScale = qsetSystemInit->value("PixelScale").toDouble();
            }
            settings.opticparams.fFOVCenterX = dOptCenterX;
            settings.opticparams.fFOVCenterY = dOptCenterY;
            settings.opticparams.fPixelScale = dPixelScale;
            qsetSystemInit->endGroup();
            /// [CameraSettings]
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("GrabBinning")
                && qsetSystemInit->contains("SubInFullCenterX")
                && qsetSystemInit->contains("SubInFullCenterY")
                && qsetSystemInit->contains("GrabWidth")
                && qsetSystemInit->contains("GrabHeight")
                && qsetSystemInit->contains("ExposureTime")
                && qsetSystemInit->contains("Gain")
                && qsetSystemInit->contains("Trigger")
                && qsetSystemInit->contains("FrameFrequency"))
            {
                m_pGParam->m_SGrabberData.bSub = true;
                m_pGParam->m_SGrabberData.iFullBinning = qsetSystemInit->value("GrabBinning").toInt();
                m_pGParam->m_SGrabberData.iSubInFullCenterX = qsetSystemInit->value("SubInFullCenterX").toInt();
                m_pGParam->m_SGrabberData.iSubInFullCenterY= qsetSystemInit->value("SubInFullCenterY").toInt();
                m_pGParam->m_SGrabberData.iSubWidth = qsetSystemInit->value("GrabWidth").toInt();
                m_pGParam->m_SGrabberData.iSubHeight = qsetSystemInit->value("GrabHeight").toInt();
                m_pGParam->m_SGrabberData.dExposureTime = qsetSystemInit->value("ExposureTime").toDouble() >= 1.3 ? qsetSystemInit->value("ExposureTime").toDouble() : 1.3;
                m_pGParam->m_SGrabberData.dExposureTime = ui.checkBox_TrackProc->isChecked() ? 45 : m_pGParam->m_SGrabberData.dExposureTime;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() >= 1.0 ? qsetSystemInit->value("Gain").toDouble() : 1.0;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() <= 10.0 ? qsetSystemInit->value("Gain").toDouble() : 10.0;
                m_pGParam->m_SGrabberData.iTriggerMode = qsetSystemInit->value("Trigger").toInt();
                m_pGParam->m_SGrabberData.dFrameFrequency = qsetSystemInit->value("FrameFrequency").toDouble();
            }
            qsetSystemInit->endGroup();
            /// [TrackSettings]
            qsetSystemInit->beginGroup("TrackSettings");
            if (qsetSystemInit->contains("ThreashLiving")
                && qsetSystemInit->contains("NumFramesLiving")
                && qsetSystemInit->contains("Radius")
                && qsetSystemInit->contains("RatioFOV")
                && qsetSystemInit->contains("ThreashStarMode")
                && qsetSystemInit->contains("ThreashGazeMode")
                && qsetSystemInit->contains("AutoDecide")
                && qsetSystemInit->contains("ThreashMEO")
                && qsetSystemInit->contains("SpdLow_AE")
                && qsetSystemInit->contains("SpdHigh_AE"))
            {
                settings.fThreashLiving = qsetSystemInit->value("ThreashLiving").toFloat();
                settings.iNumFramesLiving = qsetSystemInit->value("NumFramesLiving").toInt();
                settings.fRadius = qsetSystemInit->value("Radius").toFloat();
                settings.fRatioFOV = qsetSystemInit->value("RatioFOV").toFloat();
                settings.fThreashStarMode = qsetSystemInit->value("ThreashStarMode").toFloat();
                settings.fThreashGazeMode = qsetSystemInit->value("ThreashGazeMode").toFloat();
                settings.bAutoDecide = qsetSystemInit->value("AutoDecide").toBool();
                settings.fThreashMEO = qsetSystemInit->value("ThreashMEO").toFloat();
                settings.fSpdLow_AE = qsetSystemInit->value("SpdLow_AE").toFloat();
                settings.fSpdHigh_AE = qsetSystemInit->value("SpdHigh_AE").toFloat();
                settings.fThreash_AE = qsetSystemInit->value("Threash_AE").toFloat();
            }
            qsetSystemInit->endGroup();
            /// 相机控制
            m_pGParam->m_SGrabberData.bWindowEN = true;
            if (m_pGParam->m_SGrabberData.iWorkingStatus == STATUS_OK)
            {
                if (m_uiIndexTrackMode == 0
                        || m_uiIndexTrackMode == 1)
                {
                    QThread::msleep(5);
                    m_pGrabber->SetWindowEN(m_pGParam->m_SGrabberData.iSubHeight, m_pGParam->m_SGrabberData.iSubInFullCenterY - m_pGParam->m_SGrabberData.iSubHeight / 2);
                }
                m_uiIndexTrackMode = ui.comboBox_TrackMode->currentIndex();
                if (m_fExpTime != m_pGParam->m_SGrabberData.dExposureTime)
                {
                    m_fExpTime = m_pGParam->m_SGrabberData.dExposureTime;
                    QThread::msleep(5);
                    QString qstrExpTime = QString::number(m_pGParam->m_SGrabberData.dExposureTime, 'f', 3);
                    m_pGrabber->SetExpTime(qstrExpTime);
                }
                if (m_fGain != m_pGParam->m_SGrabberData.dGain)
                {
                    QThread::msleep(5);
                    QString qstrGain = QString::number(m_pGParam->m_SGrabberData.dGain, 'f', 3);
                    m_pGrabber->SetGain(qstrGain);
                }
                m_pGParam->m_SCommExposureData.iTriggerModeSend = TRIGGERMODE_FREE;
                m_pGParam->m_SCommExposureData.dFrameFrequencySend = 1;
            }
            /// 图像处理控制
            m_pImageProcessor->InitProc(PROC_DIRECT, TRACK_SC,
                                        m_pGParam->m_SGrabberData.iSubWidth,
                                        m_pGParam->m_SGrabberData.iSubHeight,
                                        1, 1024, 1024, m_pImageProcessor->GetProcessMode(), settings);
            uiBlobDown = 20;
            uiBlobUp = 15000;
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
            m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
            /// 控件显示
            ui.textEdit_TrackInfo->clear();
            ui.textEdit_TrackInfo->append("[OpticSettings]");
            qstrLine.sprintf("OptCenterX = %.3f", dOptCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("OptCenterY = %.3f", dOptCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("PixelScale = %.9f", dPixelScale);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[CameraSettings]");
            qstrLine.sprintf("GrabBinning = %d", m_pGParam->m_SGrabberData.iFullBinning);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SubInFullCenterX = %d", m_pGParam->m_SGrabberData.iSubInFullCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SubInFullCenterY = %d", m_pGParam->m_SGrabberData.iSubInFullCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabWidth = %d", m_pGParam->m_SGrabberData.iSubWidth);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabHeight = %d", m_pGParam->m_SGrabberData.iSubHeight);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ExposureTime = %.3f", m_pGParam->m_SGrabberData.dExposureTime);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Gain = %.3f", m_pGParam->m_SGrabberData.dGain);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Trigger = %d", m_pGParam->m_SGrabberData.iTriggerMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("FrameFrequency = %d", m_pGParam->m_SGrabberData.dFrameFrequency);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[TrackSettings]");
            qstrLine.sprintf("ThreashLiving = %.3f", settings.fThreashLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("NumFramesLiving = %d", settings.iNumFramesLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Radius = %.3f", settings.fRadius);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("RatioFOV = %.3f", settings.fRatioFOV);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashStarMode = %.3f", settings.fThreashStarMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashGazeMode = %.3f", settings.fThreashGazeMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("AutoDecide = %d", settings.bAutoDecide ? 1 : 0);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashMEO = %.3f", settings.fThreashMEO);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdLow_AE = %.3f", settings.fSpdLow_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdHigh_AE = %.3f", settings.fSpdHigh_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Threash_AE = %.3f", settings.fThreash_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            m_pLog->InfoMsg(QStringLiteral("用户设置'星校（开窗）'模式."));
        }
        break;
    case 4: // "手动提取（全帧）"
        iProcMode = PROC_DIRECT;
        iTrackMode = TRACK_MANUAL;
        /// 配置文件读取
        qsetSystemInit = new QSettings("/home/dps/IniFiles/Proc_Manual.ini", QSettings::IniFormat);
        if (qsetSystemInit)
        {
            /// [OpticSettings]
            qsetSystemInit->beginGroup("OpticSettings");
            if (qsetSystemInit->contains("OptCenterX")
                && qsetSystemInit->contains("OptCenterY")
                && qsetSystemInit->contains("PixelScale"))
            {
                dOptCenterX = qsetSystemInit->value("OptCenterX").toDouble();
                dOptCenterY = qsetSystemInit->value("OptCenterY").toDouble();
                dPixelScale = qsetSystemInit->value("PixelScale").toDouble();
            }
            settings.opticparams.fFOVCenterX = dOptCenterX;
            settings.opticparams.fFOVCenterY = dOptCenterY;
            settings.opticparams.fPixelScale = dPixelScale;
            qsetSystemInit->endGroup();
            /// [CameraSettings]
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("GrabBinning")
                && qsetSystemInit->contains("GrabWidth")
                && qsetSystemInit->contains("GrabHeight")
                && qsetSystemInit->contains("ExposureTime")
                && qsetSystemInit->contains("Gain")
                && qsetSystemInit->contains("Trigger")
                && qsetSystemInit->contains("FrameFrequency"))
            {
                m_pGParam->m_SGrabberData.bSub = false;
                m_pGParam->m_SGrabberData.iFullBinning = qsetSystemInit->value("GrabBinning").toInt();
                m_pGParam->m_SGrabberData.iFullWidth = qsetSystemInit->value("GrabWidth").toInt();
                m_pGParam->m_SGrabberData.iFullHeight = qsetSystemInit->value("GrabHeight").toInt();
                m_pGParam->m_SGrabberData.dExposureTime = qsetSystemInit->value("ExposureTime").toDouble() >= 1.3 ? qsetSystemInit->value("ExposureTime").toDouble() : 1.3;
//                m_pGParam->m_SGrabberData.dExposureTime = (ui.checkBox_TrackProc->isChecked() && m_pGParam->m_SGrabberData.dExposureTime < 1500) ? 1500 : m_pGParam->m_SGrabberData.dExposureTime;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() >= 1.0 ? qsetSystemInit->value("Gain").toDouble() : 1.0;
                m_pGParam->m_SGrabberData.dGain = qsetSystemInit->value("Gain").toDouble() <= 10.0 ? qsetSystemInit->value("Gain").toDouble() : 10.0;
                m_pGParam->m_SGrabberData.iTriggerMode = qsetSystemInit->value("Trigger").toInt();
                m_pGParam->m_SGrabberData.dFrameFrequency = qsetSystemInit->value("FrameFrequency").toDouble();
            }
            qsetSystemInit->endGroup();
            /// [TrackSettings]
            qsetSystemInit->beginGroup("TrackSettings");
            if (qsetSystemInit->contains("ThreashLiving")
                && qsetSystemInit->contains("NumFramesLiving")
                && qsetSystemInit->contains("Radius")
                && qsetSystemInit->contains("RatioFOV")
                && qsetSystemInit->contains("ThreashStarMode")
                && qsetSystemInit->contains("ThreashGazeMode")
                && qsetSystemInit->contains("AutoDecide")
                && qsetSystemInit->contains("ThreashMEO")
                && qsetSystemInit->contains("SpdLow_AE")
                && qsetSystemInit->contains("SpdHigh_AE"))
            {
                settings.fThreashLiving = qsetSystemInit->value("ThreashLiving").toFloat();
                settings.iNumFramesLiving = qsetSystemInit->value("NumFramesLiving").toInt();
                settings.fRadius = qsetSystemInit->value("Radius").toFloat();
                settings.fRatioFOV = qsetSystemInit->value("RatioFOV").toFloat();
                settings.fThreashStarMode = qsetSystemInit->value("ThreashStarMode").toFloat();
                settings.fThreashGazeMode = qsetSystemInit->value("ThreashGazeMode").toFloat();
                settings.bAutoDecide = qsetSystemInit->value("AutoDecide").toBool();
                settings.fThreashMEO = qsetSystemInit->value("ThreashMEO").toFloat();
                settings.fSpdLow_AE = qsetSystemInit->value("SpdLow_AE").toFloat();
                settings.fSpdHigh_AE = qsetSystemInit->value("SpdHigh_AE").toFloat();
                settings.fThreash_AE = qsetSystemInit->value("Threash_AE").toFloat();
            }
            qsetSystemInit->endGroup();
            /// 相机控制
            m_pGParam->m_SGrabberData.bWindowEN = false;
            if (m_pGParam->m_SGrabberData.iWorkingStatus == STATUS_OK)
            {
                if (m_uiIndexTrackMode == 2
                        || m_uiIndexTrackMode == 3
                        || m_uiIndexTrackMode == 10000000)
                {
                    QThread::msleep(5);
                    m_pGrabber->SetWindowEN(m_pGParam->m_SGrabberData.iFullHeight, 0);
                }
                m_uiIndexTrackMode = ui.comboBox_TrackMode->currentIndex();
                if (m_fExpTime != m_pGParam->m_SGrabberData.dExposureTime)
                {
                    m_fExpTime = m_pGParam->m_SGrabberData.dExposureTime;
                    QThread::msleep(5);
                    QString qstrExpTime = QString::number(m_pGParam->m_SGrabberData.dExposureTime, 'f', 3);
                    m_pGrabber->SetExpTime(qstrExpTime);
                }
                if (m_fGain != m_pGParam->m_SGrabberData.dGain)
                {
                    QThread::msleep(5);
                    QString qstrGain = QString::number(m_pGParam->m_SGrabberData.dGain, 'f', 3);
                    m_pGrabber->SetGain(qstrGain);
                }
                m_pGParam->m_SCommExposureData.iTriggerModeSend = TRIGGERMODE_FREE;
                m_pGParam->m_SCommExposureData.dFrameFrequencySend = 1;
            }
            /// 图像处理控制
            m_pImageProcessor->InitProc(PROC_DIRECT, TRACK_MANUAL,
                                        m_pGParam->m_SGrabberData.iFullWidth,
                                        m_pGParam->m_SGrabberData.iFullHeight,
                                        1, 1024, 1024, m_pImageProcessor->GetProcessMode(), settings);
            uiBlobDown = 8;
            uiBlobUp = 2500;
            ui.lineEdit_BlobDown->setText(QString::number(uiBlobDown));
            ui.lineEdit_BlobUp->setText(QString::number(uiBlobUp));
            m_pImageProcessor->SetBlobAreaLimit(uiBlobDown, uiBlobUp);
            /// 控件显示
            ui.textEdit_TrackInfo->clear();
            ui.textEdit_TrackInfo->append("[OpticSettings]");
            qstrLine.sprintf("OptCenterX = %.3f", dOptCenterX);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("OptCenterY = %.3f", dOptCenterY);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("PixelScale = %.9f", dPixelScale);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[CameraSettings]");
            qstrLine.sprintf("GrabBinning = %d", m_pGParam->m_SGrabberData.iFullBinning);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabWidth = %d", m_pGParam->m_SGrabberData.iFullWidth);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("GrabHeight = %d", m_pGParam->m_SGrabberData.iFullHeight);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ExposureTime = %.3f", m_pGParam->m_SGrabberData.dExposureTime);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Gain = %.3f", m_pGParam->m_SGrabberData.dGain);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Trigger = %d", m_pGParam->m_SGrabberData.iTriggerMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("FrameFrequency = %d", m_pGParam->m_SGrabberData.dFrameFrequency);
            ui.textEdit_TrackInfo->append(qstrLine);
            ui.textEdit_TrackInfo->append("\n[TrackSettings]");
            qstrLine.sprintf("ThreashLiving = %.3f", settings.fThreashLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("NumFramesLiving = %d", settings.iNumFramesLiving);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Radius = %.3f", settings.fRadius);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("RatioFOV = %.3f", settings.fRatioFOV);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashStarMode = %.3f", settings.fThreashStarMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashGazeMode = %.3f", settings.fThreashGazeMode);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("AutoDecide = %d", settings.bAutoDecide ? 1 : 0);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("ThreashMEO = %.3f", settings.fThreashMEO);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdLow_AE = %.3f", settings.fSpdLow_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("SpdHigh_AE = %.3f", settings.fSpdHigh_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            qstrLine.sprintf("Threash_AE = %.3f", settings.fThreash_AE);
            ui.textEdit_TrackInfo->append(qstrLine);
            m_pLog->InfoMsg(QStringLiteral("用户设置'手动提取（全帧）'踪模式."));
        }
        break;
    default:
        break;
    }

    if(m_pGParam->m_SImageProcessorData.bWindowEN != m_pGParam->m_SGrabberData.bWindowEN)
    {
        m_pGParam->m_SImageProcessorData.bWindowEN = m_pGParam->m_SGrabberData.bWindowEN;
        m_pImageProcessor->Init_TWDW();
        m_pImageProcessor->Init_Photometry();
        m_pImageProcessor->Set_Photometry();
    }

    m_pGParam->m_SOpticData.fOptCenterX = dOptCenterX;
    m_pGParam->m_SOpticData.fOptCenterY = dOptCenterY;
    m_pGParam->m_SOpticData.fPixelScale = dPixelScale;

    m_settings = settings;

    if (qsetSystemInit) delete qsetSystemInit;

    emit SignalDispFit();

    if (!ui.checkBox_MCCtrl->isChecked())     ui.comboBox_TrackMode->setEnabled(true);

    QThread::msleep(5);
    if (bGrabbing) m_pGrabber->ResumeGrab();
    if (bEnabled) ui.checkBox_ImageGrab->setEnabled(true);

    ui.checkBox_MCCtrl->setEnabled(true);
    ui.checkBox_TrackProc->setEnabled(true);
    ui.checkBox_TrackSave->setEnabled(true);
    ui.checkBox_DrawStarMap->setEnabled(true);
    ui.checkBox_DistValid->setEnabled(true);
    ui.checkBox_SpdValid->setEnabled(true);
    ui.comboBox_TrackMode_2->setEnabled(true);
    ui.checkBox_AutoScale->setEnabled(true);
    ui.checkBox_EnhanceDisp->setEnabled(true);
    ui.checkBox_TrackInfoEN->setEnabled(true);
    ui.pushButton_TrackParamsRefresh->setEnabled(true);
    ui.pushButton_TrackParamsSet->setEnabled(true);
    ui.lineEdit_BlobUp->setEnabled(true);
    ui.lineEdit_BlobDown->setEnabled(true);
    m_bParamsLoading = false;
}

void UI_CtrlPad::WriteParams()
{
    QString qstrText = ui.textEdit_TrackInfo->toPlainText();
    QString qstrFileName;
    switch (ui.comboBox_TrackMode->currentIndex())
    {
    case 0: // "凝视搜索/引导跟踪（全帧）"
        qstrFileName = "/home/dps/IniFiles/Proc_NoCloseLoop.ini";
        m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_NoCloseLoop.ini'."));
        break;
    case 1: // "恒动搜索（全帧）"
        qstrFileName = "/home/dps/IniFiles/Proc_Search.ini";
        m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_Search.ini'."));
        break;
    case 2: // "闭环跟踪 （开窗）"
        qstrFileName = "/home/dps/IniFiles/Proc_CloseLoop.ini";
        m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_CloseLoop.ini'."));
        break;
    case 3: // "星校（开窗）"
        qstrFileName = "/home/dps/IniFiles/Proc_StarCalib.ini";
        m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_StarCalib.ini'."));
        break;
    case 4: // "手动提取（全帧）"
        qstrFileName = "/home/dps/IniFiles/Proc_Manual.ini";
        m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_Manual.ini'."));
        break;
    default:
        break;
    }
    QFile file(qstrFileName);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate))
    {
        QMessageBox::warning(this,"sdf","can't open",QMessageBox::Yes);
    }
    QTextStream stream(&file);
    stream << qstrText << endl;
    file.close();
}

void UI_CtrlPad::on_SignalMsgPrint(QString qstrMsg)
{
    ui.textEdit_Log->append(qstrMsg);
}

void UI_CtrlPad::on_SignalChangeUITrackMode(int iMode)
{
    switch (iMode)
    {
    case 0:
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像宽高不匹配，图像加载失败.\n建议切换为“...（全帧）”模式."));
        break;
    case 1:
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像宽高不匹配，图像加载失败.\n建议切换为“...（开窗）”模式."));
        break;
    case 2:
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("图像宽高不匹配，图像加载失败.\n建议检查图像文件头是否有效."));
        break;
    default:
        break;
    }
}

void UI_CtrlPad::on_SignalDisplay()
{
    int iProcMode = m_pImageProcessor->GetProcMode();
    int iTrackMode = m_pImageProcessor->GetTrackMode();
    if ((iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
      || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC)
      || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_MANUAL))
    {
        sTargetInfo info;
        vector<sTargetInfo> vectInfo;
        m_pImageProcessor->GetVectTargetTWDWInfo(vectInfo);
        if(!vectInfo.empty())
            info = vectInfo[0];

        if (info.vectInfoInFrame.size() > 3 && info.bLiving)
        {
            QString qstrTargetID, qstrPosXY, qstrZXDW, qstrTWDW, qstrTargetMag, qstrTime;
            sResPacket resPacket = info.vectResPacket[info.vectResPacket.size() - 1];
            qstrTargetID = iTrackMode == TRACK_SC ? "Calib" : info.qstrTargetID;
            std::pair<float,float> pairfPosXY = resPacket.pairfTargetPosInFrame;
            qstrPosXY.sprintf("%d, %d", (unsigned int)(pairfPosXY.first), (unsigned int)(pairfPosXY.second));
            std::pair<float,float> pairfZXDW = resPacket.pairfTargetPosZXDW;
            qstrZXDW.sprintf("%.4f, %.4f", pairfZXDW.first, pairfZXDW.second);
            std::pair<float,float> pairfTWDW = resPacket.pairfTargetPosTWDW;
            qstrTWDW.sprintf("%.4f, %.4f", pairfTWDW.first, pairfTWDW.second);
            float fTargetMag = resPacket.fTargetMvGDCL;
            qstrTargetMag = QString::number(fTargetMag, 'f', 2);
            qstrTime.sprintf("%04d-%02d-%02d  %02d:%02d:%02d.%03d",
                             resPacket.stimeFrame.iYear,
                             resPacket.stimeFrame.iMonth,
                             resPacket.stimeFrame.iDay,
                             resPacket.stimeFrame.iHour,
                             resPacket.stimeFrame.iMinute,
                             resPacket.stimeFrame.iSecond,
                             resPacket.stimeFrame.iMillisecond);

            ui.tableWidget_TargetInfo->clear();
            QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
            ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
            ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
            ui.tableWidget_TargetInfo->setItem(0, 0, new QTableWidgetItem(qstrTargetID));
            ui.tableWidget_TargetInfo->setItem(0, 1, new QTableWidgetItem(qstrPosXY));
            ui.tableWidget_TargetInfo->setItem(0, 2, new QTableWidgetItem(qstrZXDW));
            ui.tableWidget_TargetInfo->setItem(0, 3, new QTableWidgetItem(qstrTWDW));
            ui.tableWidget_TargetInfo->setItem(0, 4, new QTableWidgetItem(qstrTargetMag));
            ui.tableWidget_TargetInfo->setItem(0, 5, new QTableWidgetItem(qstrTime));
            for (int i = 0; i < 6; i++)
            {
                ui.tableWidget_TargetInfo->item(0, i)->setTextAlignment(Qt::AlignCenter);
                if (resPacket.bValid)
                    ui.tableWidget_TargetInfo->item(0, i)->setForeground(QBrush(QColor(18, 18, 110)));
                else
                    ui.tableWidget_TargetInfo->item(0, i)->setForeground(QBrush(QColor(128, 128, 128)));
            }
        }
    }
    else if ((iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
             || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO))
    {
        vector<sTargetInfo> vectInfo;
        m_pImageProcessor->GetVectTargetTWDWInfo(vectInfo);
        int iNumFrames = m_pImageProcessor->GetNumFrames();
        if (iNumFrames >= 4)
        {
            if (vectInfo.size() > 0)
            {
                ui.tableWidget_TargetInfo->clear();
                QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
                ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
                ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
            }
//            int iSize = vectInfo.size() > 50 ? 50 : vectInfo.size();
            int iSize = vectInfo.size();
            for (int i = 0; i < iSize; i++)
            {
                sTargetInfo info = vectInfo[i];

                if (info.bLiving)
                {
                    QString qstrTargetID, qstrPosXY, qstrZXDW, qstrTWDW, qstrTargetMag, qstrTime;
                    sResPacket resPacket = info.vectResPacket[info.vectResPacket.size() - 1];
                    qstrTargetID = info.qstrTargetID;
                    std::pair<float,float> pairfPosXY = resPacket.pairfTargetPosInFrame;
                    qstrPosXY.sprintf("%d, %d", (unsigned int)(pairfPosXY.first), (unsigned int)(pairfPosXY.second));
                    std::pair<float,float> pairfZXDW = resPacket.pairfTargetPosZXDW;
                    qstrZXDW.sprintf("%.4f, %.4f", pairfZXDW.first, pairfZXDW.second);
                    std::pair<float,float> pairfTWDW = resPacket.pairfTargetPosTWDW;
                    qstrTWDW.sprintf("%.4f, %.4f", pairfTWDW.first, pairfTWDW.second);
                    float fTargetMag = resPacket.fTargetMvGDCL;
                    qstrTargetMag = QString::number(fTargetMag, 'f', 2);
                    qstrTime.sprintf("%04d-%02d-%02d  %02d:%02d:%02d.%03d",
                                     resPacket.stimeFrame.iYear,
                                     resPacket.stimeFrame.iMonth,
                                     resPacket.stimeFrame.iDay,
                                     resPacket.stimeFrame.iHour,
                                     resPacket.stimeFrame.iMinute,
                                     resPacket.stimeFrame.iSecond,
                                     resPacket.stimeFrame.iMillisecond);

                    ui.tableWidget_TargetInfo->setItem(i, 0, new QTableWidgetItem(qstrTargetID));
                    ui.tableWidget_TargetInfo->setItem(i, 1, new QTableWidgetItem(qstrPosXY));
                    ui.tableWidget_TargetInfo->setItem(i, 2, new QTableWidgetItem(qstrZXDW));
                    ui.tableWidget_TargetInfo->setItem(i, 3, new QTableWidgetItem(qstrTWDW));
                    ui.tableWidget_TargetInfo->setItem(i, 4, new QTableWidgetItem(qstrTargetMag));
                    ui.tableWidget_TargetInfo->setItem(i, 5, new QTableWidgetItem(qstrTime));
                    for (int j = 0; j < 6; j++)
                    {
                        ui.tableWidget_TargetInfo->item(i, j)->setTextAlignment(Qt::AlignCenter);
                        if (resPacket.bValid)
                            ui.tableWidget_TargetInfo->item(i, j)->setForeground(QBrush(QColor(18, 18, 110)));
                        else
                            ui.tableWidget_TargetInfo->item(i, j)->setForeground(QBrush(QColor(128, 128, 128)));
                    }
                }
            }
        }
    }
}

void UI_CtrlPad::on_SignalMCSet(float fExpTime, int iIndexComboTrackMode, bool bTrack, bool bImageSave)
{
    if (!m_bMCCtrlSetting && !m_bParamsLoading)
    {
        on_SignalMCSetTrackMode(iIndexComboTrackMode);
        on_SignalMCSetExp(fExpTime);
        on_SignalMCSetImageSave(bImageSave);
        on_SignalMCSetTrack(bTrack);
        on_SignalMCSetTaskChanged();
    }
}

void UI_CtrlPad::on_SignalMCSetExp(float fExpTime)
{
    float fExpTime1 = fExpTime >= 1.3 ? fExpTime : 1.3;
    if (ui.checkBox_MCCtrl->isChecked()
            && abs(fExpTime1-m_fExpTime) >= 0.001)
    {
        int iTrackMode = m_pImageProcessor->GetTrackMode();
        bool bTrackProc = m_pImageProcessor->GetTrackProc();
        if (!((iTrackMode == TRACK_LEO || iTrackMode == TRACK_SC) && bTrackProc))
        {
            QString qstrFileName;
            switch (ui.comboBox_TrackMode->currentIndex())
            {
            case 0: // "凝视搜索/引导跟踪（全帧）"
                qstrFileName = "/home/dps/IniFiles/Proc_NoCloseLoop.ini";
                m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_NoCloseLoop.ini'."));
                break;
            case 1: // "恒动搜索（全帧）"
                qstrFileName = "/home/dps/IniFiles/Proc_Search.ini";
                m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_Search.ini'."));
                break;
            case 2: // "闭环跟踪 （开窗）"
                qstrFileName = "/home/dps/IniFiles/Proc_CloseLoop.ini";
                m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_CloseLoop.ini'."));
                break;
            case 3: // "星校（开窗）"
                qstrFileName = "/home/dps/IniFiles/Proc_StarCalib.ini";
                m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_StarCalib.ini'."));
                break;
            case 4: // "手动提取（全帧）"
                qstrFileName = "/home/dps/IniFiles/Proc_Manual.ini";
                m_pLog->InfoMsg(QStringLiteral("用户修改配置文件:'Proc_Manual.ini'."));
                break;
            default:
                break;
            }
            QSettings* qsetSystemInit = new QSettings(qstrFileName, QSettings::IniFormat);
            qsetSystemInit->beginGroup("CameraSettings");
            if (qsetSystemInit->contains("ExposureTime"))
            {
//                m_fExpTime = fExpTime1;
                qsetSystemInit->setValue("ExposureTime", QString::number(fExpTime1, 'f', 3));
            }
            qsetSystemInit->endGroup();
            LoadParams();
        }
    }
}

void UI_CtrlPad::on_SignalMCSetTrackMode(int iIndexComboTrackMode)
{
    if (ui.checkBox_MCCtrl->isChecked())
    {
        if ((ui.comboBox_TrackMode->currentIndex() != iIndexComboTrackMode)
                && (iIndexComboTrackMode == 0
                || iIndexComboTrackMode == 1
                || iIndexComboTrackMode == 2
                || iIndexComboTrackMode == 3))
        {
            ui.comboBox_TrackMode->setCurrentIndex(iIndexComboTrackMode);

            LoadParams();
            WriteParams();
            m_pImageProcessor->Init_TWDW();

            ui.tableWidget_TargetInfo->clear();
            QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
            ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
            ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));

            QString qstrPrint = QStringLiteral("主控切换跟踪模式：");
            switch(iIndexComboTrackMode)
            {
            case 0:
                qstrPrint += QStringLiteral("凝视搜索/引导跟踪（全帧）");
                break;
            case 1:
                qstrPrint += QStringLiteral("恒动搜索（全帧）");
                break;
            case 2:
                qstrPrint += QStringLiteral("闭环跟踪 （开窗）");
                break;
            case 3:
                qstrPrint += QStringLiteral("星校（开窗）");
                break;
            default:
                break;
            }
            m_pLog->InfoMsg(qstrPrint);
        }
    }
}

void UI_CtrlPad::on_SignalMCSetTrack(bool bTrackProc)
{
    if (ui.checkBox_MCCtrl->isChecked())
    {
        if (m_pImageProcessor->GetTrackProc() != bTrackProc)
        {
            ui.checkBox_TrackProc->setChecked(bTrackProc);
            if (bTrackProc)
            {
                bool bGrabbing = m_pGrabber->GetGrabStatus();
                if(bGrabbing) m_pGrabber->PauseGrab();
                switch (ui.comboBox_TrackMode->currentIndex())
                {
                case 0: // "凝视搜索/引导跟踪（全帧）"
                    m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 1: // "恒动搜索（全帧）"
                    m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    break;
                case 2: // "闭环跟踪 （开窗）"
                    m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    if (m_pGParam->m_SGrabberData.dExposureTime != 45)
                    {
                        LoadParams();
                        WriteParams();
                    }
                    break;
                case 3: // "星校（开窗）"
                    m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                    m_pImageProcessor->SetTrackProc(true);
                    if (m_pGParam->m_SGrabberData.dExposureTime != 45)
                    {
                        LoadParams();
                        WriteParams();
                    }
                    break;
                default:
                    break;
                }
                m_pImageReplayer->SetPause();
                m_pGrabber->ResumeGrab();
                m_pLog->InfoMsg(QStringLiteral("主控开启搜索或跟踪."));
            }
            else
            {
                bool bGrabbing = m_pGrabber->GetGrabStatus();
                if(bGrabbing) m_pGrabber->PauseGrab();
                m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
                m_pImageProcessor->SetTrackProc(false);
                m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
                m_pLog->InfoMsg(QStringLiteral("主控停止搜索或跟踪."));
            }
            ui.tableWidget_TargetInfo->clear();
            QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
            ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
            ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
        }
    }
}

void UI_CtrlPad::on_SignalMCSetImageSave(bool bImageSave)
{
    if (ui.checkBox_MCCtrl->isChecked())
    {
        if (ui.checkBox_ImageSave->isChecked() != bImageSave)
        {
            ui.checkBox_ImageSave->setChecked(bImageSave);           
            m_pImageStorage->SetStore(bImageSave);
            if (bImageSave)
            {
                m_pLog->InfoMsg(QStringLiteral("主控开启图像存储."));
            }
            else
            {
                m_pLog->InfoMsg(QStringLiteral("主控停止图像存储."));
            }
        }
    }
}

void UI_CtrlPad::on_SignalMCSetTaskChanged()
{
    if (ui.checkBox_MCCtrl->isChecked() && m_pGParam->m_SNetMasterControlData.bTaskChanged)
    {
        m_pGParam->m_SNetMasterControlData.bTaskChanged = false;
        /// Stop TrackProc
        m_pImageProcessor->SetTrackProc(false);
        m_pGParam->m_SImageProcessorData.bDoubleClicked = false;
        /// Start TrackProc
        switch (ui.comboBox_TrackMode->currentIndex())
        {
        case 0: // "凝视搜索/引导跟踪（全帧）"
            m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            break;
        case 1: // "恒动搜索（全帧）"
            m_pImageProcessor->InitTracker(TRACK_GEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            break;
        case 2: // "闭环跟踪 （开窗）"
            m_pImageProcessor->InitTracker(TRACK_LEO, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            break;
        case 3: // "星校（开窗）"
            m_pImageProcessor->InitTracker(TRACK_SC, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            break;
        default:
            m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
            break;
        }
        QThread::msleep(500);
        m_pImageProcessor->SetTrackProc(true);
        ui.tableWidget_TargetInfo->clear();
        QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
        ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
        ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));
        QString qstrPrint = QStringLiteral("主控切换任务：") + m_pGParam->m_SNetMasterControlData.qstrTargetID;
        m_pLog->WarnMsg(qstrPrint);
        m_pGrabber->TrainCam();
        m_pLog->InfoMsg(QStringLiteral("训练相机."));
    }
}

void UI_CtrlPad::on_SignalManualInit()
{
    unsigned int uiTrackMode = m_pImageProcessor->GetTrackMode();
    if (uiTrackMode == TRACK_MANUAL)
    {
        m_pImageProcessor->InitTracker(TRACK_MANUAL, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
    }
}

void UI_CtrlPad::on_SignalLabelMouseClicked(float fClickX, float fClickY)
{
    QString qstrPrint = QStringLiteral("用户点击图像位置为: ( ") + QString::number((int)fClickX) + " , " + QString::number((int)fClickY) + " )";
    ui.lineEdit_TargetXPos->setText(QString::number(fClickX, 'f', 4));
    ui.lineEdit_TargetYPos->setText(QString::number(fClickY, 'f', 4));
    m_pairPosManual = make_pair(fClickX, fClickY);
    m_pLog->InfoMsg(qstrPrint);
}

void UI_CtrlPad::on_checkBox_AutoScale_clicked(void)
{
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    unsigned int uiScaleMax = ui.lineEdit_ScaleUp->text().toUInt();
    unsigned int uiScaleMin = ui.lineEdit_ScaleDown->text().toUInt();
    if (uiScaleMin >= 0 && uiScaleMax > uiScaleMin)
    {
        m_pImageProcessor->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    }
    else
    {
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
    }
    ui.lineEdit_ScaleUp->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleDown->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleUp->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleDown->setEnabled(!bAutoScale);
}

void UI_CtrlPad::on_lineEdit_ScaleDown_returnPressed()
{
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    unsigned int uiScaleMax = ui.lineEdit_ScaleUp->text().toUInt();
    unsigned int uiScaleMin = ui.lineEdit_ScaleDown->text().toUInt();
    if (uiScaleMin >= 0 && uiScaleMin <= 16384 && uiScaleMax >= 0 && uiScaleMax <= 16384 && uiScaleMax > uiScaleMin)
    {
    }
    else
    {
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
        uiScaleMax = 5000;
        uiScaleMin = 1000;
    }
    m_pImageProcessor->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    ui.lineEdit_ScaleUp->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleUp->setText(QString::number(uiScaleMax));
    ui.lineEdit_ScaleDown->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleDown->setText(QString::number(uiScaleMin));
    ui.horizontalSlider_ScaleUp->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleUp->setValue(uiScaleMax);
    ui.horizontalSlider_ScaleDown->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleDown->setValue(uiScaleMin);
    emit SignalDispStatus();
}

void UI_CtrlPad::on_lineEdit_ScaleUp_returnPressed()
{
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    unsigned int uiScaleMax = ui.lineEdit_ScaleUp->text().toUInt();
    unsigned int uiScaleMin = ui.lineEdit_ScaleDown->text().toUInt();
    if (uiScaleMin >= 0 && uiScaleMin <= 16384 && uiScaleMax >= 0 && uiScaleMax <= 16384 && uiScaleMax > uiScaleMin)
    {
    }
    else
    {
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("输入值超限."));
        uiScaleMax = 5000;
        uiScaleMin = 1000;
    }
    m_pImageProcessor->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    ui.lineEdit_ScaleUp->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleUp->setText(QString::number(uiScaleMax));
    ui.lineEdit_ScaleDown->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleDown->setText(QString::number(uiScaleMin));
    ui.horizontalSlider_ScaleUp->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleUp->setValue(uiScaleMax);
    ui.horizontalSlider_ScaleDown->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleDown->setValue(uiScaleMin);
    emit SignalDispStatus();
}

void UI_CtrlPad::on_horizontalSlider_ScaleDown_sliderMoved(int position)
{
    unsigned int uiScaleMin = position;
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    unsigned int uiScaleMax = ui.lineEdit_ScaleUp->text().toUInt();
    if (uiScaleMax > uiScaleMin)
    {
    }
    else
    {
        uiScaleMin = uiScaleMax - 1;
        ui.horizontalSlider_ScaleDown->setValue(uiScaleMin);
    }
    ui.lineEdit_ScaleDown->setText(QString::number(uiScaleMin));
    m_pImageProcessor->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    ui.lineEdit_ScaleUp->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleDown->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleUp->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleDown->setEnabled(!bAutoScale);
}

void UI_CtrlPad::on_horizontalSlider_ScaleUp_sliderMoved(int position)
{
    unsigned int uiScaleMax = position;
    bool bAutoScale = ui.checkBox_AutoScale->isChecked();
    unsigned int uiScaleMin = ui.lineEdit_ScaleDown->text().toUInt();
    if (uiScaleMax > uiScaleMin)
    {
    }
    else
    {
        uiScaleMax = uiScaleMin + 1;
        ui.horizontalSlider_ScaleUp->setValue(uiScaleMax);
    }
    ui.lineEdit_ScaleUp->setText(QString::number(uiScaleMax));
    m_pImageProcessor->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    ui.lineEdit_ScaleUp->setEnabled(!bAutoScale);
    ui.lineEdit_ScaleDown->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleUp->setEnabled(!bAutoScale);
    ui.horizontalSlider_ScaleDown->setEnabled(!bAutoScale);
}


void UI_CtrlPad::on_horizontalSlider_ScaleUp_sliderReleased()
{
    emit SignalDispStatus();
}

void UI_CtrlPad::on_horizontalSlider_ScaleDown_sliderReleased()
{
    emit SignalDispStatus();
}

void UI_CtrlPad::on_checkBox_DrawStarMap_clicked()
{
    m_pImageProcessor->Init_TWDW();
    emit SignalDrawStarMap(ui.checkBox_DrawStarMap->isChecked());
}

void UI_CtrlPad::on_comboBox_TrackMode_2_activated(int)
{
    m_pImageProcessor->SetCenterMode(!(ui.comboBox_TrackMode_2->currentIndex() == 1));
}

void UI_CtrlPad::on_checkBox_MCCtrl_clicked()
{
//    QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("MC Ctrl."));
    m_bMCCtrlSetting = true;

    m_pImageReplayer->SetPause();
    bool bGrabbing = m_pGrabber->GetGrabStatus();
    if(bGrabbing) m_pGrabber->PauseGrab();
    if (ui.checkBox_MCCtrl->isChecked())
    {
        ui.checkBox_ImageGrab->setChecked(true);
        ui.checkBox_ImageGrab->setEnabled(false);
        ui.pushButton_PlayPathBrowse->setEnabled(false);
        ui.pushButton_Next->setEnabled(false);
        ui.pushButton_Pause->setEnabled(false);
        ui.pushButton_Previous->setEnabled(false);
        ui.pushButton_NextAuto->setEnabled(false);
        ui.pushButton_PreviousAuto->setEnabled(false);
        ui.pushButton_DistCurve->setEnabled(false);
        ui.pushButton_DistCurve_2->setEnabled(false);
        ui.comboBox_TrackMode->setEnabled(false);
        ui.checkBox_TrackInfoEN->setChecked(false);
        ui.checkBox_TrackInfoEN->setEnabled(false);
        ui.textEdit_TrackInfo->setReadOnly(true);
        ui.pushButton_TrackParamsSet->setEnabled(false);
        ui.pushButton_TrackParamsRefresh->setEnabled(false);
        ui.checkBox_MatchTimeSave->setEnabled(false);
        ui.checkBox_ImageSave->setEnabled(false);
        ui.pushButton_SavePathBrowse->setEnabled(false);
        ui.lineEdit_AddProc->setEnabled(false);
        ui.lineEdit_AddFrameNum->setEnabled(false);
        ui.pushButton_AddRePeat->setEnabled(false);
        m_pLog->InfoMsg(QStringLiteral("用户选定数据处理系统受主控系统控制."));
    }
    else
    {
        ui.checkBox_ImageGrab->setChecked(false);
        ui.checkBox_ImageGrab->setEnabled(true);
        ui.pushButton_PlayPathBrowse->setEnabled(true);
        ui.pushButton_Next->setEnabled(true);
        ui.pushButton_Pause->setEnabled(true);
        ui.pushButton_Previous->setEnabled(true);
        ui.pushButton_NextAuto->setEnabled(true);
        ui.pushButton_PreviousAuto->setEnabled(true);
        ui.pushButton_DistCurve->setEnabled(true);
        ui.pushButton_DistCurve_2->setEnabled(true);
        ui.comboBox_TrackMode->setEnabled(true);
        ui.checkBox_TrackInfoEN->setChecked(false);
        ui.checkBox_TrackInfoEN->setEnabled(true);
        ui.textEdit_TrackInfo->setReadOnly(true);
        ui.pushButton_TrackParamsSet->setEnabled(true);
        ui.pushButton_TrackParamsRefresh->setEnabled(true);
        ui.checkBox_MatchTimeSave->setEnabled(true);
        ui.checkBox_ImageSave->setEnabled(true);
        ui.pushButton_SavePathBrowse->setEnabled(true);
        ui.lineEdit_AddProc->setEnabled(true);
        ui.lineEdit_AddFrameNum->setEnabled(true);
        ui.pushButton_AddRePeat->setEnabled(true);
        m_pLog->InfoMsg(QStringLiteral("用户选定数据处理系统不受主控系统控制."));
    }

    ui.checkBox_TrackProc->setChecked(false);
    m_pImageProcessor->InitTracker(TRACK_INIT, m_settings, m_pGParam->m_SNetMasterControlData.qstrTaskID.toUInt());
    m_pImageProcessor->SetTrackProc(false);
    m_pGParam->m_SImageProcessorData.bDoubleClicked = false;

    ui.tableWidget_TargetInfo->clear();
    QStringList qstrlistTitle = {"目标编号", "图像位置", "轴系定位 / °", "天文定位 / °", "光度 / mV", "测量时间"};
    ui.tableWidget_TargetInfo->setHorizontalHeaderLabels(qstrlistTitle);
    ui.tableWidget_TargetInfo->horizontalHeader()->setFont(QFont("song", 13));

    m_bMCCtrlSetting = false;
}

void UI_CtrlPad::on_pushButton_TrainCam_clicked()
{
    m_pGrabber->TrainCam();
}


void UI_CtrlPad::on_checkBox_Dark_clicked(bool checked)
{
    m_pImageProcessor->SetDark(checked);
}

void UI_CtrlPad::on_checkBox_DispBW_clicked(bool checked)
{
    m_pImageProcessor->SetDispBW(checked);
    emit SignalDispStatus();
}

void UI_CtrlPad::on_comboBox_ThreshBW_currentIndexChanged(int index)
{
    float fRatio = 5.0;
    switch (index) {
        case 0:
            fRatio = 5.0;
            break;
        case 1:
            fRatio = 6.0;
            break;
        case 2:
            fRatio = 7.0;
            break;
        case 3:
            fRatio = 8.0;
            break;
        default:
            break;
    }
    m_pImageProcessor->SetStdRatio(fRatio);
}

void UI_CtrlPad::on_checkBox_LooseThresh_clicked(bool checked)
{
    m_pGParam->m_SImageProcessorData.bLooseThresh = checked;
}

void UI_CtrlPad::on_checkBox_ForceRePoint_clicked(bool checked)
{
    m_pGParam->m_SDataProcessorData.bForceRePoint = checked;
}

void UI_CtrlPad::on_comboBox_FrameFreq_currentIndexChanged(int index)
{
    float fFrameInterval = 5000.0;
    switch (index) {
        case 0:
            fFrameInterval = 4000.0;
            break;
        case 1:
            fFrameInterval = 5000.0;
            break;
        case 2:
            fFrameInterval = 6000.0;
            break;
        case 3:
            fFrameInterval = 8000.0;
            break;
        case 4:
            fFrameInterval = 10000.0;
            break;
        default:
            break;
    }
    m_pGrabber->SetFrameInterval(fFrameInterval);
}

void UI_CtrlPad::on_pushButton_CleanData_clicked()
{
    QString qstrPathDefault = "/storage/DPS/TASK";
    QString qstrPath = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择待清洗的GTW与GDJ文件路径"), qstrPathDefault);
    if (!qstrPath.isEmpty())
    {
        QStringList qstrlistFilesGTW = FindFiles(qstrPath, QStringList() << "*.GTW");
        int iDeleteGTW = 0;
        DeleteLessFile(qstrlistFilesGTW, iDeleteGTW);

        QStringList qstrlistFilesGDJ = FindFiles(qstrPath, QStringList() << "*.GDJ");
        int iDeleteGDJ= 0;
        DeleteLessFile(qstrlistFilesGDJ, iDeleteGDJ);

        QString qstrInfo = QStringLiteral("GTW文件共计：") + QString::number(qstrlistFilesGTW.size())
                + "\nGTW文件有效：" + QString::number(qstrlistFilesGTW.size()-iDeleteGTW)
                + "\nGTW文件清除：" +  QString::number(iDeleteGTW)
                + QStringLiteral("\n\nGDJ文件共计：") + QString::number(qstrlistFilesGDJ.size())
                + "\nGDJ文件有效：" + QString::number(qstrlistFilesGDJ.size()-iDeleteGDJ)
                + "\nGDJ文件清除：" +  QString::number(iDeleteGDJ);
        QMessageBox::information(NULL, QStringLiteral("消息"), qstrInfo);
    }
}

QStringList UI_CtrlPad::FindFiles(const QString &startDir, const QStringList &filters)
{
    QStringList names;
    QDir dir(startDir);

    const auto files = dir.entryList(filters, QDir::Files);
    for (const QString &file : files)
    {
        names += startDir + "/" + file;
    }

    const auto subdirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (const QString &subdir : subdirs)
    {
        names += FindFiles(startDir + "/" + subdir, filters);
    }

    return names;
}

void UI_CtrlPad::DeleteLessFile(QStringList qstrlistFiles, int& iDelete)
{
    for (int i = 0; i < qstrlistFiles.size(); i++)
    {
        vector<string> tmp_files;
        ifstream infile(qstrlistFiles[i].toLocal8Bit().data());
        string lineContent;
        unsigned int uiValidLines = 0;
        while (getline( infile, lineContent, '\n' ))
        {
            uiValidLines++;
            if (lineContent == "C END\r")
            {
                break;
            }
        }
        infile.close();
        if (uiValidLines <= 17)
        {
            iDelete++;
            QFile file(qstrlistFiles[i]);
            file.remove();
        }
    }
}

void UI_CtrlPad::on_radioButton_ZoomFit_clicked(bool checked)
{
    if (checked)
        emit SignalZoom(0);
}

void UI_CtrlPad::on_radioButton_ZoomOut_clicked(bool checked)
{
    if (checked)
        emit SignalZoom(1);
}

void UI_CtrlPad::on_radioButton_ZoomIn_clicked(bool checked)
{
    if (checked)
        emit SignalZoom(2);
}

void UI_CtrlPad::changeRaDecTrackParams()
{
    double dRaThresh = ui.lineEdit_RaThresh->text().toDouble() / 3600.0;
    double dDecThresh = ui.lineEdit_DecThresh->text().toDouble() / 3600.0;
    double dRaSpdThresh = ui.lineEdit_RaSpdThresh->text().toDouble() / 3600.0;
    double dDecSpdThresh = ui.lineEdit_DecSpdThresh->text().toDouble() / 3600.0;
    m_pImageProcessor->setRaDecThresh(dRaThresh, dDecThresh, dRaSpdThresh, dDecSpdThresh);
    QMessageBox::information(this, "修改Ra、Dec、RaSpd、DecSpd阈值", "修改成功");
}

void UI_CtrlPad::on_lineEdit_DecThresh_returnPressed()
{
    changeRaDecTrackParams();
}

void UI_CtrlPad::on_lineEdit_RaThresh_returnPressed()
{
    changeRaDecTrackParams();
}

void UI_CtrlPad::on_lineEdit_RaSpdThresh_returnPressed()
{
    changeRaDecTrackParams();
}

void UI_CtrlPad::on_lineEdit_DecSpdThresh_returnPressed()
{
    changeRaDecTrackParams();
}


void UI_CtrlPad::on_pushButton_PythonExE_clicked()
{
    m_pGParam->m_STrackParams.qstrExEPath = QFileDialog::getOpenFileName(nullptr, "选择python解释器", "/home", "All Files (*);;Text Files (*.txt)");
    ui.lineEdit_PythonExE->setText(m_pGParam->m_STrackParams.qstrExEPath);
}

void UI_CtrlPad::on_pushButton_pyPath_clicked()
{
    m_pGParam->m_STrackParams.qstrPYPath = QFileDialog::getExistingDirectory(nullptr, "选择py脚本文件路径", "/home");
    ui.lineEdit_pyPath->setText(m_pGParam->m_STrackParams.qstrPYPath);
    LoadPYFiles(m_pGParam->m_STrackParams.qstrPYPath);
}

void UI_CtrlPad::LoadPYFiles(QString PYPath)
{
    QDir qDirPy(PYPath);
    if(qDirPy.exists())
    {
        ui.comboBox_pyFIles->clear();
        m_pGParam->m_STrackParams.qListPYFile.clear();
        m_pGParam->m_STrackParams.qListPYFile = qDirPy.entryList(QStringList("*.py"), QDir::Files | QDir::Readable, QDir::Name);

        for (QString& cur : m_pGParam->m_STrackParams.qListPYFile)
            ui.comboBox_pyFIles->addItem(cur);
    }
}

void UI_CtrlPad::on_lineEdit_AddFrameNum_returnPressed()
{
    if(ui.lineEdit_AddFrameNum->text().toUInt() > 0)
    {
        if(QMessageBox::question(this, "询问", "是否修改叠加帧数？", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            m_pGParam->m_SAddImage.uiAddFrameNum = ui.lineEdit_AddFrameNum->text().toUInt();
    }
    else
    {
         QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("“叠加帧数”输入错误,修改失败.\n仅允许设置为>0的整数."));
    }
    ui.lineEdit_AddFrameNum->setText(QString::number(m_pGParam->m_SAddImage.uiAddFrameNum));
}

void UI_CtrlPad::on_pushButton_AddRePeat_clicked()
{
    if(QMessageBox::question(this, QStringLiteral("询问"), QStringLiteral("是否叠加"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        m_pGParam->m_SAddImage.bAddRepeat = true;
        ui.pushButton_Next->setEnabled(false);
        ui.pushButton_Pause->setEnabled(false);
        ui.pushButton_Previous->setEnabled(false);
        ui.pushButton_NextAuto->setEnabled(false);
        ui.pushButton_PreviousAuto->setEnabled(false);
        ui.lineEdit_ImagePlayPath->setEnabled(false);
        ui.pushButton_PlayPathBrowse->setEnabled(false);
        ui.pushButton_AddRePeat->setEnabled(false);
    }
    emit SignalAddImage();
}

void UI_CtrlPad::on_SignalAddEnding()
{
    m_pGParam->m_SAddImage.bAddRepeat = false;
    ui.pushButton_Next->setEnabled(true);
    ui.pushButton_Pause->setEnabled(true);
    ui.pushButton_Previous->setEnabled(true);
    ui.pushButton_NextAuto->setEnabled(true);
    ui.pushButton_PreviousAuto->setEnabled(true);
    ui.lineEdit_ImagePlayPath->setEnabled(true);
    ui.pushButton_PlayPathBrowse->setEnabled(true);
    ui.pushButton_AddRePeat->setEnabled(true);
}

void UI_CtrlPad::on_SignalAddProc(int iSeqCut, int iNumTotal)
{
    ui.lineEdit_AddProc->setText(QString("%1 / %2").arg(iSeqCut).arg(iNumTotal));
}

void UI_CtrlPad::on_checkBox_LockDisp_clicked(bool checked)
{
    m_pGParam->m_bDispMode = checked;
    ui.radioButton_ZoomIn->setEnabled(checked);
    ui.radioButton_ZoomFit->setEnabled(checked);
    ui.radioButton_ZoomOut->setEnabled(checked);
}

void UI_CtrlPad::on_checkBox_TrackAlgorithm_clicked(bool checked)
{
    m_pGParam->m_SImageProcessorData.bTrackAlgorithm = checked;
    ui.groupBox_3->setEnabled(checked);
}

void UI_CtrlPad::LoadSource()
{
    QFileInfo fileInfo(m_pGParam->m_SImageReplayerData.qstrCurFileName);
    QString fileName = fileInfo.fileName(); // 获取文件名
    QString absolutePath = fileInfo.absolutePath(); // 获取文件的绝对路径（目录路径）
    QString sourceFile = absolutePath + "/Sources/" + fileName + "Sources.csv";
    QFile outfile(sourceFile);
    if (!outfile.open(QIODevice::ReadOnly | QIODevice::Text))
        qDebug() << "Could not open file for reading.";
    QTextStream outText(&outfile);

    // 读取第一行并检查处理是否成功
    QString firstLine = outText.readLine();
    if (!firstLine.startsWith("Successful"))
        qDebug() << "Processing was not successful.";
    else
    {
        QString line = outText.readLine();
        QStringList fields = line.split(",");
        for (int row = 0; row < fields.size(); row++)
        {
            QTableWidgetItem *valueItem = new QTableWidgetItem(fields[row]);
            ui.tableWidget_SourceInfo->setItem(row, 1, valueItem);
        }
    }
}

void UI_CtrlPad::on_pushButton_ManualSource_clicked()
{
    QString pythonScript = m_pGParam->m_STrackParams.qstrPYPath + "/extract.py";
    QStringList functionArguments;
    functionArguments << m_pGParam->m_SImageReplayerData.qstrCurFileName << QString::fromStdString(std::to_string((int)m_pairPosManual.first)) << QString::fromStdString(std::to_string((int)m_pairPosManual.second)); // 函数参数，比如文件路径
    QProcess pythonProcess;
    pythonProcess.start(m_pGParam->m_STrackParams.qstrExEPath, QStringList() << pythonScript << functionArguments);
    if (!pythonProcess.waitForStarted())
        qDebug() << "Failed to start Python process.";
    if (!pythonProcess.waitForFinished())
        qDebug() << "Failed to finish Python process.";
    QString output = pythonProcess.readAllStandardOutput();
    qDebug() << "Python output:" << output;
    LoadSource();
}

void UI_CtrlPad::on_checkBox_SourceInfoEN_clicked(bool checked)
{
    if (ui.checkBox_SourceInfoEN->isChecked())
    {
        ui.tableWidget_SourceInfo->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
        ui.tableWidget_SourceInfo->setFrameShape(QFrame::NoFrame);
    }
    else
    {
        ui.tableWidget_SourceInfo->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui.tableWidget_SourceInfo->setFrameShape(QFrame::StyledPanel);
    }
}

void UI_CtrlPad::on_pushButton_SourceInfoSet_clicked()
{

}
