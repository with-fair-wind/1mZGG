#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLCDNumber>
#include <math.h>
#include <QKeyEvent>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QDebug>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QtCharts>
#include "ui_UI_CtrlPad.h"
#include "GlobalParameter.h"
#include "StandardThread.h"
#include "QImage8UC1BufferPackage.h"
#include "Grabber.h"
#include "ImageStorage.h"
#include "ImageReplayer.h"
#include "ImageProcessor.h"
//#include "DataProcessor.h"
#include "TrackDataStorage.h"
#include "MyLog.h"
#include "UI_DistCurve.h"
#include "getperiod.h"


class UI_CtrlPad : public QMainWindow
{
Q_OBJECT

/// 函数
public:
    UI_CtrlPad(QWidget *parent = 0);
    ~UI_CtrlPad();

protected:
    void keyPressEvent(QKeyEvent* event);
    void closeEvent(QCloseEvent* event);

private:
    void UIInit(void);
    void UpdataUIParams(void);
    void LoadParams(void);
    void WriteParams(void);
    void on_SignalMCSetExp(float fExpTime);
    void on_SignalMCSetTrackMode(int iIndexComboTrackMode);
    void on_SignalMCSetTrack(bool bTrack);
    void on_SignalMCSetImageSave(bool bImageSave);
    void on_SignalMCSetTaskChanged(void);
    QStringList FindFiles(const QString &startDir, const QStringList &filters);
    void DeleteLessFile(QStringList qstrlistFiles, int& iDelete);

signals:
    void SignalCloseCuard();
    void SignalClose();
    void SignalDispFit();
    void SignalDrawStarMap(bool);
    void SignalZoom(int);

private slots :
    void on_checkBox_ImageGrab_clicked(void);
    void on_pushButton_SavePathBrowse_clicked(void);
    void on_checkBox_MatchTimeSave_clicked(void);
    void on_checkBox_ImageSave_clicked(void);
    void on_pushButton_PlayPathBrowse_clicked(void);
    void on_pushButton_Previous_clicked(void);
    void on_pushButton_PreviousAuto_clicked(void);
    void on_pushButton_Pause_clicked(void);
    void on_pushButton_NextAuto_clicked(void);
    void on_pushButton_Next_clicked(void);
    void on_checkBox_EnhanceDisp_clicked(void);
    void on_lineEdit_BlobDown_returnPressed(void);
    void on_lineEdit_BlobUp_returnPressed(void);
    void on_comboBox_TrackMode_activated(int);
    void on_pushButton_TrackParamsRefresh_clicked(void);
    void on_pushButton_TrackParamsSet_clicked(void);
    void on_checkBox_TrackSave_clicked(void);
    void on_checkBox_TrackInfoEN_clicked(void);
    void on_checkBox_DistValid_clicked(void);
    void on_checkBox_SpdValid_clicked(void);
    void on_checkBox_TrackProc_clicked(bool bChecked);
    void on_pushButton_DistCurve_clicked(void);
    void on_pushButton_DistCurve_2_clicked(void);
    void on_checkBox_AutoScale_clicked(void);
    void on_lineEdit_ScaleDown_returnPressed(void);
    void on_lineEdit_ScaleUp_returnPressed(void);
    void on_horizontalSlider_ScaleDown_sliderMoved(int position);
    void on_horizontalSlider_ScaleUp_sliderMoved(int position);
    void on_checkBox_DrawStarMap_clicked(void);
    void on_comboBox_TrackMode_2_activated(int);
    void on_checkBox_MCCtrl_clicked(void);
    void on_pushButton_TrainCam_clicked(void);
    void on_UITimerOut(void);
    void on_SignalMsgPrint(QString);
    void on_SignalChangeUITrackMode(int iMode);  // iMode==0：切换为‘...（全帧）’;iMode==1：切换为‘...（开窗）’;iMode==2：图像宽高不匹配
    void on_SignalDisplay(void);
    void on_SignalMCSet(float fExpTime, int iIndexComboTrackMode, bool bTrack, bool bImageSave);
    void on_SignalManualInit(void);
    void on_SignalLabelMouseClicked(QPoint qptPos);
    void on_checkBox_Dark_clicked(bool checked);
    void on_checkBox_DispBW_clicked(bool checked);
    void on_comboBox_ThreshBW_currentIndexChanged(int index);
    void on_checkBox_LooseThresh_clicked(bool checked);
    void on_checkBox_ForceRePoint_clicked(bool checked);
    void on_comboBox_FrameFreq_currentIndexChanged(int index);
    void on_pushButton_CleanData_clicked();
    void on_radioButton_ZoomFit_clicked(bool checked);
    void on_radioButton_ZoomOut_clicked(bool checked);
    void on_radioButton_ZoomIn_clicked(bool checked);

private:
    Ui::MainWindow ui;
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    Grabber* m_pGrabber;	// 相机采集对象指针
    ImageReplayer* m_pImageReplayer;	// 图像回放对象指针
    ImageProcessor* m_pImageProcessor;	// 图像处理对象指针
//    DataProcessor* m_pDataProcessor;
    ImageStorage* m_pImageStorage;	// 图像存储对象指针
    TrackDataStorage* m_pTrackDataStorage;	// 数据存储对象指针
    bool m_bCrossDisplay;
    QTimer* m_qtimerUI;
    MyLog* m_pLog;
    unsigned int m_uiIndexTrackMode;
    UI_DistCurve* m_UI_DistCurve;
    sTrackingSettings m_settings;
    float m_fExpTime;
    float m_fGain;
    int m_iValueScaleDown;
    int m_iValueScaleUp;
    QString m_qstrPathReplay;
    bool m_bMCCtrlSetting;
    bool m_bParamsLoading;
};

#endif // MAINWINDOW_H
