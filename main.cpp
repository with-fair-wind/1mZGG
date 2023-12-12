#include <iostream>

#include "SingleApplication.h"
#include "GlobalParameter.h"
#include "UI_CtrlPad.h"
#include "UI_DispPad.h"
#include "UI_InitDlg.h"
#include "UI_DispPadS.h"
#include "CommDisplay.h"
#include "CommExposure.h"
#include "CommMasterControl.h"
#include "CommServo.h"
#include "Grabber.h"
#include "ImageProcessor.h"
//#include "DataProcessor.h"
#include "ImageReplayer.h"
#include "ImageStorage.h"
#include "TrackDataStorage.h"
#include "NetImageSender.h"
#include "NetErrorDiagnose.h"
#include "NetAtmos.h"
#include "NetApp.h"
#include "MyLog.h"
#include <QMessageBox>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QThread>
#include <QtWidgets/QFileDialog>


#include <QDebug>
int main(int argc, char *argv[])
{
    /// 单实例运行
    SingleApplication app(argc, argv);
//    QApplication app(argc, argv);
    UI_InitDlg initdlgMain;
    initdlgMain.show();
    if (!app.isRunning())
    {
        /// 功能类实例化
        GlobalParameter* pglobalparameterMain = GlobalParameter::GetInstance();	// 全局参数单一实例化
        //// ping storage ////
        QProcess cmd;
        cmd.start("ping 192.168.0.100");
        cmd.waitForFinished(1000);
        QByteArray qbaRead = cmd.readAllStandardOutput();
        pglobalparameterMain->m_SCommNetSettings.bStorageConnected = !qbaRead.isEmpty();
        //////////////////////
        QString qstrFileName = "/home/dps/IniFiles/SystemInit.ini";
        switch (pglobalparameterMain->ReadInitFile(qstrFileName))	// 读取配置文件
        {
            case INIT_ERROR_COMMNETSETTINGS:
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("配置文件读取错误.\n应用程序无法启动."));
            case INIT_ERROR_PATH:
                QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("配置文件读取错误.\n应用程序无法启动."));
            default:
                break;
        }
        ///// mount storage /////
        if (pglobalparameterMain->m_SCommNetSettings.bStorageConnected)
        {
            QProcess proc;
            proc.start("/home/dps/IniFiles/mount_dps.sh");
            proc.waitForFinished(1000);
        }
        else
        {
            pglobalparameterMain->m_SPath.qstrDataStorePath = "/home/dps/DPS_DATA/TASK";
//            pglobalparameterMain->m_SPath.qstrDataStorePath = "/home/dps/DPS_DATA_TEMP/DPS/TASK";
        }
        //////////////////////
        MyLog mylogMain;
        CommDisplay commdisplayMain;	// 机下光端机显示
        CommExposure commexposureMain;	// 机下光端机曝光
        CommMasterControl commmastercontrolMain;	// 机下光端机曝光
        NetErrorDiagnose netedMain;
        CommCamera commcameraMain;
        Grabber grabberMain;	// 相机采集
        ImageReplayer imagereplayerMain;	// 图像回放
        NetExchange netexchangeMain;
        ImageProcessor imageprocessorMain;	// 图像处理
//        DataProcessor dataprocessorMain;    // 数据处理
        CommServo commservoMain;	// 机下光端机曝光
        ImageStorage imagestorageMain;	// 图像存储
        TrackDataStorage trackdatastorageMain;	// 跟踪数据存储
        UI_CtrlPad ctrlpadMain;	// 主对话框
        UI_DispPad disppadMain;
        UI_DispPadS disppadsMain;
        NetImageSender netisMain;
        NetAtmos netatmosMain;
        NetApp netappMain;

        /// CommDisplay信号连接
        QObject::connect(&commdisplayMain, SIGNAL(Signal25Hz()), &commexposureMain, SLOT(sendInfo()));
        /// CommExposure信号连接

        /// CommMasterControl信号连接
        QObject::connect(&commmastercontrolMain, SIGNAL(SignalMCSet(float, int, bool, bool)), &ctrlpadMain, SLOT(on_SignalMCSet(float, int, bool, bool)));
        /// Grabber信号连接
        QObject::connect(&grabberMain, SIGNAL(SignalStartGrab(unsigned int,unsigned int)), &imagestorageMain, SLOT(on_SignalStartGrab(unsigned int,unsigned int)));
        QObject::connect(&grabberMain, SIGNAL(SignalGrabData()), &imageprocessorMain, SLOT(on_SignalGrabData()));
        QObject::connect(&grabberMain, SIGNAL(SignalGrabInit(unsigned int, unsigned int)), &imageprocessorMain, SLOT(on_SignalGrabInit(unsigned int, unsigned int)));
        /// ImageProcessor信号连接
        QObject::connect(&imageprocessorMain, SIGNAL(SignalTrackData()), &trackdatastorageMain, SLOT(on_SignalTrackData()));
//        QObject::connect(&imageprocessorMain, SIGNAL(SignalTrackData()), &dataprocessorMain, SLOT(on_SignalTrackData()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalDisplay()), &disppadMain, SLOT(on_SignalDisplay()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalSend()), &netisMain, SLOT(on_SignalSend()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalGrabDataRotate()), &imagestorageMain, SLOT(on_SignalGrabDataRotate()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalTrackData()), &commservoMain, SLOT(sendInfo()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalTrackData()), &commmastercontrolMain, SLOT(sendInfo()));
        QObject::connect(&imageprocessorMain, SIGNAL(SignalDisplay()), &disppadsMain, SLOT(on_SignalDisplay()));

        QObject::connect(&imageprocessorMain, SIGNAL(SignalDisplay()), &ctrlpadMain, SLOT(on_SignalDisplay()));

        /// ImageReplayer信号连接
        QObject::connect(&imagereplayerMain, SIGNAL(SignalReplayData()), &imageprocessorMain, SLOT(on_SignalReplayData()));
        QObject::connect(&imagereplayerMain, SIGNAL(SignalReplayInit(unsigned int, unsigned int)), &imageprocessorMain, SLOT(on_SignalReplayInit(unsigned int, unsigned int)));
        QObject::connect(&imagereplayerMain, SIGNAL(SignalChangeUITrackMode(int)), &ctrlpadMain, SLOT(on_SignalChangeUITrackMode(int)));
        /// DataProcessor信号连接
//        QObject::connect(&dataprocessorMain, SIGNAL(SignalDisplay()), &ctrlpadMain, SLOT(on_SignalDisplay()));
        /// MyLog信号连接
        QObject::connect(&mylogMain, SIGNAL(SignalMsgPrint(QString)), &ctrlpadMain, SLOT(on_SignalMsgPrint(QString)));
        /// UI_CtrlPad信号连接
        QObject::connect(&ctrlpadMain, SIGNAL(SignalClose()), &disppadMain, SLOT(on_SignalClose()));
        QObject::connect(&ctrlpadMain, SIGNAL(SignalClose()), &disppadsMain, SLOT(on_SignalClose()));
        QObject::connect(&ctrlpadMain, SIGNAL(SignalCloseCuard()), &netappMain, SLOT(on_SignalCloseCuard()));
//        QObject::connect(&ctrlpadMain, SIGNAL(SignalDispFit()), &disppadMain, SLOT(on_Fit_clicked()));
        QObject::connect(&ctrlpadMain, SIGNAL(SignalZoom(int)), &disppadMain, SLOT(on_SignalZoom(int)));
        QObject::connect(&ctrlpadMain, SIGNAL(SignalDrawStarMap(bool)), &disppadMain, SLOT(on_SignalDrawStarMap(bool)));
        QObject::connect(&ctrlpadMain, SIGNAL(SignalDispStatus()), &imageprocessorMain, SLOT(on_SignalDispStatus()));
        /// UI_DispPad
        QObject::connect(&disppadMain, SIGNAL(SignalManualInit()), &ctrlpadMain, SLOT(on_SignalManualInit()));
        QObject::connect(&disppadMain, SIGNAL(SignalLabelMouseClicked(QPoint)), &ctrlpadMain, SLOT(on_SignalLabelMouseClicked(QPoint)));
        QObject::connect(&disppadMain, SIGNAL(SignalCropDisp(bool)), &disppadsMain, SLOT(on_SignalCropDisp(bool)));

        QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
            cmd.terminate();
            cmd.waitForFinished();
        });

        /// 界面运行
        initdlgMain.hide();
        ctrlpadMain.show();
        disppadMain.show();
//        disppadsMain.show();

        return app.exec();
    }
    else
    {
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("已经有一个应用程序实例正在运行."));
        return NULL;
    }
}
