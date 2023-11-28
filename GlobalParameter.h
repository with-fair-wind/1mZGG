#ifndef GLOBALPARAMETER_H
#define GLOBALPARAMETER_H

#include <QSettings>
#include <QPoint>
#include <QPointF>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QtCore/QMutex>
#include "DefinedMacro.h"

/// 通信端口
struct SCommNetSettings
{
    QString qstrPortDisplay = "/dev/ttyUSB0S";	// 机下光端机显示数据端口
    QString qstrPortExposure = "/dev/ttyUSB1S";	// 机下光端机曝光数据端口
    QString qstrPortMasterControl = "/dev/ttyUSB2S"; // 主控串行端口
    QString qstrPortServo = "/dev/ttyUSB3S";  //
    QString qstrIPLocal = "192.168.1.1";	// 本地IP
    QString qstrIPImageTrans = "192.168.1.2";   // 图像传输IP
    int iPortImageTrans = 4000;	// 图像传输端口
    QString qstrIPMasterControl = "192.168.1.3";    // 主控IP
    int iPortMasterControl = 5000; // 主控端口
    QString qstrIPErrDiag = "192.168.1.4";	// 故障诊断IP
    int iPortErrDiag = 6000;	// 故障诊断端口
    int iPortAtmos = 6000;
    QString qstrIPExchange = "192.168.1.5";
    int iPortExchangeGXTC = 10002;
    int iPortExchangeGDCL = 10003;
    bool bStorageConnected = false;
};

/// 路径
struct SPath
{
    QString qstrDataStorePath = "/home/dps/DPS_Data";	// 数据保存路径
    QString qstrInitFileName = "SystemInit.ini";	// 配置文件路径
};

/// 光学参数
struct SOpticData
{
    float fOptCenterX = 3072;  // 图像光学中心X
    float fOptCenterY = 3072;  // 图像光学中心Y
    float fPixelScale = 0.0003453;  // 图像像素比例尺(°/pixel)
    float fRotateDeg = 0;
};

/// 站址参数
struct SObsParams
{
    int iObsID=999;
    float fLongitude = 101.1684653; // 经度(°)
    float fLatitude = 25.4839054;   // 纬度(°)
    bool bEW = true;
    bool bNS = true;    // 南北半球(true:北半球;false:南半球)
    float fAltitude = 2092.7968;    // 海拔高度(m)
};

/// 触发端口状态
struct SIOTriggerData
{
    int iStatusPort = STATUS_INIT;	// 端口状态
    int iDTRStatus = STATUS_INIT;	// DTR触发状态
    int iDTRTimesPerSec = 0;	// DTR每秒触发次数
    int iRTSStatus = STATUS_INIT;	// RTS触发状态
    int iRTSTimesPerSec = 0;	// RTS每秒触发次数
};

struct SExposureDisplayData
{
    int iYMDYear = 0;	// 年月日的年
    int iYMDMonth = 0;	// 年月日的月
    int iYMDDay = 0;	// 年月日的日
    int iDayRecv = 0;	// 天(本年累计天数)
    int iHourRecv = 0;	// 时
    int iMinuteRecv = 0;	// 分
    int iSecondRecv = 0;	// 秒
    int iMilliSecondRecv = 0;	// 毫秒
    int iMicroSecondRecv = 0;	// 微秒
    double dAzimuthDegreeTotalRecv = 0;	// 方位度全部
    int iAzimuthDegreeRecv = 0;	// 方位度
    int iAzimuthMinuteRecv = 0;	// 方位分
    int iAzimuthSecondRecv = 0;	// 方位秒
    double dElevationDegreeTotalRecv = 0;	// 俯仰度全部
    int iElevationDegreeRecv = 0;	// 俯仰度
    int iElevationMinuteRecv = 0;	// 俯仰分
    int iElevationSecondRecv = 0;	// 俯仰秒
    float fTemp = 12.6;    // temperature/sheshidu
    float fAtmosP = 78980.0;  // atmos pressure/Pa
    float fHumidity = 0.365;  // humidity
};

struct SCommExposureData
{
    /// 固有的通讯状态(不受qmutex保护)
    int iStatusPort = STATUS_INIT;	// 端口状态
    int iCommErrorRecvLost = STATUS_INIT;	// 通信错误-接收丢失帧数超限
    int iCommErrorRecvTimeMark = STATUS_INIT;	// 通信错误-时标连续丢失超限
    unsigned char* paucRecvBuffer = NULL;	// 接收缓存指针
    int iRecvBytes = 0;	// 接收帧字节数
    int iRecvFramePerSec = 0;	// 实际接收帧数
    int iCommErrorSend = STATUS_INIT;	// 通信错误-发送异常
    unsigned char* paucSendBuffer = NULL;	// 发送缓存指针
    int iSendBytes = 0;	// 发送帧字节数
    int iSendFramePerSec = 0;	// 实际发送帧数
    bool bSaveData = false;	// 保存数据
    /// 接收数据(受qmutexRecv保护)
    QMutex qmutexRecv;	// 接收数据锁
    SExposureDisplayData sexpdispdataRecv;	// 当前帧数据
    bool bReady = false;
    /// 发送数据(受qmutexSend保护)
    QMutex qmutexSend;	// 发送数据锁
    int iTriggerModeSend = TRIGGERMODE_FREE;	// 触发方式(详见宏定义)
    double dFrameFrequencySend = 1.0;	// 帧频(详见宏定义)
    double dExposureTimeSend = 100;	// 曝光时间 ms
};

struct SCommDisplayData
{
    /// 固有的通讯状态(不受qmutex保护)
    int iStatusPort = STATUS_INIT;	// 端口状态
    int iCommErrorRecvLost = STATUS_INIT;	// 通信错误-接收丢失帧数超限
    int iCommErrorRecvTimeMark = STATUS_INIT;	// 通信错误-时标连续丢失超限
    unsigned char* paucRecvBuffer = NULL;	// 接收缓存指针
    int iRecvBytes = 0;	// 接收帧字节数
    int iRecvFramePerSec = 0;	// 实际接收帧数
    int iCommErrorSend = STATUS_INIT;	// 通信错误-发送异常
    unsigned char* paucSendBuffer = NULL;	// 发送缓存指针
    int iSendBytes = 0;	// 发送帧字节数
    int iSendFramePerSec = 0;	// 实际发送帧数
    bool bSaveData = false;	// 保存数据
    /// 接收数据(受qmutexRecv保护)
    QMutex qmutexRecv;	// 接收数据锁
    SExposureDisplayData sexpdispdataRecv;  //(不受qmutexRecv保护)
    /// 发送数据(受qmutexSend保护)
    QMutex qmutexSend;	// 发送数据锁
};

struct SCommServoData
{
    /// 固有的通讯状态(不受qmutex保护)
    int iStatusPort = STATUS_INIT;	// 端口状态
    int iCommErrorRecvLost = STATUS_INIT;	// 通信错误-接收丢失帧数超限
    int iCommErrorRecvTimeMark = STATUS_INIT;	// 通信错误-时标连续丢失超限
    unsigned char* paucRecvBuffer = NULL;	// 接收缓存指针
    int iRecvBytes = 0;	// 接收帧字节数
    int iRecvFramePerSec = 0;	// 实际接收帧数
    int iCommErrorSend = STATUS_INIT;	// 通信错误-发送异常
    unsigned char* paucSendBuffer = NULL;	// 发送缓存指针
    int iSendBytes = 0;	// 发送帧字节数
    int iSendFramePerSec = 0;	// 实际发送帧数
    bool bSaveData = false;	// 保存数据
    /// 接收数据(受qmutexRecv保护)
    QMutex qmutexRecv;	// 接收数据锁
    /// 发送数据(受qmutexSend保护)
    QMutex qmutexSend;	// 发送数据锁
    bool bDistValid=true;
    bool bSpdValid=true;
};

struct SCommMasterControlData
{
    /// 固有的通讯状态(不受qmutex保护)
    int iStatusPort = STATUS_INIT;	// 端口状态
    int iCommErrorRecvLost = STATUS_INIT;	// 通信错误-接收丢失帧数超限
    int iCommErrorRecvTimeMark = STATUS_INIT;	// 通信错误-时标连续丢失超限
    unsigned char* paucRecvBuffer = NULL;	// 接收缓存指针
    int iRecvBytes = 0;	// 接收帧字节数
    int iRecvFramePerSec = 0;	// 实际接收帧数
    int iCommErrorSend = STATUS_INIT;	// 通信错误-发送异常
    unsigned char* paucSendBuffer = NULL;	// 发送缓存指针
    int iSendBytes = 0;	// 发送帧字节数
    int iSendFramePerSec = 0;	// 实际发送帧数
    bool bSaveData = false;	// 保存数据
    /// 接收数据(受qmutexRecv保护)
    QMutex qmutexRecv;	// 接收数据锁
    /// 发送数据(受qmutexSend保护)
    QMutex qmutexSend;	// 发送数据锁
};

struct SCommCameraData
{
    /// Grabber实例化对象指针
    void* pvoidComm;
};

struct SNetImageSenderData
{
    int iStatusConnect = STATUS_INIT;
};

struct SNetMasterControlData
{
    void* pvoidNetMasterControl = NULL;
    int iStatusConnect = STATUS_INIT;

    bool bSearch = false; // true:search; false:track
    QString qstrTaskID = "999999";
    QString qstrTargetID = "888888";
    QString qstrStartTime = "20210101080800";
    QString qstrEndTime = "20210101090900";
    bool bTaskChanged = false;
};

struct SNetExchangeData
{
    void* pvoidThis = NULL;
    int iStatusConnect = STATUS_INIT;
};

/// NetErrorDiagnose数据
struct SNetErrorDiagnoseData
{
    void* pvoidThis = NULL;
    int iStatusConnect = STATUS_INIT;	// 连接状态
    QMutex qmutexRecv;	// 接收数据锁
    QMutex qmutexSend;	// 接收数据锁
};

/// NetAtmos数据
struct sNetAtmosData
{
    void* pvoidThis = NULL;
    int iStatusConnect = STATUS_INIT;	// 连接状态
    QMutex qmutexRecv;	// 接收数据锁
    float fTemp = 17.9;    // temperature/sheshidu
    float fAtmosP = 78910.0;  // atmos pressure/Pa
    float fHumidity = 0.265;  // humidity
    QMutex qmutexSend;	// 接收数据锁
};

/// Grabber数据 (设置曝光/触发/帧频时,需更新SCommExposureData.iTriggerModeSend/iFrameFrequencySend/dExposureTime)
struct SGrabberData
{
    /// Grabber实例化对象指针
    void* pvoidGrabber = NULL;
    void* pvoidComm = NULL;
    /// 初始化参数
    bool bSub = false;
    int iFullBinning = 1;   // 全帧图像Binning
    int iFullWidth = 6144; // 全帧图像宽度
    int iFullHeight = 6144;    // 全帧图像高度
    int iSubInFullCenterX = 3072;   // 开窗图像中心X,对应Binning后的全帧图像坐标
    int iSubInFullCenterY = 3072;   // 开窗图像中心Y,对应Binning后的全帧图像坐标
    int iSubWidth = 1024; // 开窗图像宽度
    int iSubHeight = 1024;    // 开窗图像高度
    double dExposureTimeInit = 100;	// 曝光时间 ms
    double dGainInit = 1;	// 增益
    int iTriggerModeInit = TRIGGERMODE_FREE;	// 触发方式
    double dFrameFrequencyInit = 1;	// 帧频
    /// 状态
    int iWorkingStatus = STATUS_INIT;	// 相机初始化状态
    int iGrabingStatus = STATUS_INIT;
    int iInitStatus = STATUS_INIT;
    int iRecvStatus = STATUS_INIT;
    /// 数据(受qmutexGrab保护)
    QMutex qmutexGrab;	// 采集数据锁
    unsigned short* pausGrabDataCurrent = NULL;	// 当前帧数据
    unsigned short* pausGrabDataRotate = NULL;
    double dExposureTime = 100;
    double dGain = 1;	// 增益
    int iTriggerMode = TRIGGERMODE_FREE;	// 触发方式
    double dFrameFrequency = 1;	// 帧频
    SExposureDisplayData sexpdispdataGrab;
    bool bWindowEN = false;
};

/// ImageProcessor数据
struct SImageProcessorData
{
    /// ImageProcessor实例化对象指针
    void* pvoidImageProcessor = NULL;
    /// 数据
    QMutex qmutexProcess;
    SExposureDisplayData sexpdispdataDisp;
    SExposureDisplayData sexpdispdataProc;
    double dExposureTimeDisp = 100;
    int iTriggerModeDisp = TRIGGERMODE_FREE;
    double dFrameFrequencyDisp = 1;
    /// Init参数
    bool bCenterModeInit = false;	// 提取形心或质心,true:形心,false:质心
    bool bAutoScaleInit = true;  // 自动拉伸标识
    unsigned int uiScaleMaxInit = 5000;    // 拉伸最大值
    unsigned int uiScaleMinInit = 500;    // 拉伸最小值
    int iMaxAreaInit = 1000;	// 最大连通域面积
    int iMinAreaInit = 5;	// 最小连通域面积
    std::pair<float,float> pairfMousePos = std::pair<float,float>(0,0);
    std::pair<float,float> pairfClickedPos = std::pair<float,float>(0,0);
    bool bDoubleClicked = false;
    QMutex qmutexDeleteBuf;
    bool bFullLEO = false;
    float fThreshErr=2.0;
    float fRadiusT=30.0;
    float fRadiusS=50.0;
    bool bLooseThresh=false;
    /// Disp
    unsigned int uiCropWidth=0;
    unsigned int uiCropHeight=0;
    unsigned int uiNumBlob=0;
    float fRatioStd=0;
    std::pair<float,float> pairfStarSpd=std::pair<float,float>(0,0);
    unsigned int uiNumMeasure=0;
    unsigned int uiNumStarMatch=0;
    unsigned int uiProcTrackMs=0;
    float fAvr=0;
    float fStd=0;
    int iProcTime=0;

    bool bProcessMode = true;
    bool bProcessing = false;
    bool bWindowEN = false;
};

/// DataProcessor数据
struct SDataProcessorData
{
    /// DataProcessor实例化对象指针
    void* pvoidDataProcessor = NULL;
    /// 数据
    QMutex qmutexProcess;
    float fErrAzi=0;    // pixel
    float fErrEle=0;    // pixel
    float fRotate=0;    // degree
    int iNumStarMatched=0;
    int iNumStarCalc=0;
    bool bValid=false;
    bool bReady=false;
    int iTWDWTime=0;
    bool bForceRePoint=false;
};

/// 图像文件头
struct SImageFileHeader
{
    /// 备注: 先高后低
    char acImageWidth[2] = {0,0};	// 图像宽度
    char acImageHeight[2] = {0,0};	// 图像高度
    char acYear[2] = {0,0};	// 年
    char cMonth = 0;	// 月
    char cDay = 0;	// 日
    char cHour = 0;	// 时
    char cMinute = 0;	// 分
    char cSecond = 0;	// 秒
    char acMilliSecond[2] = {0,0};	// 毫秒
    char acMicroSecond[2] = {0,0};	// 微秒
    char acAzimuth[3] = {0,0,0};	// 方位度×10000
    char acElevation[3] = {0,0,0};	// 俯仰度×10000
    char acExposureTime[3] = {0,0,0};	// 曝光时间 μs
    char cTriggerMode = 0;	// 触发方式(详见宏定义)
    char acFrameFrequency[2] = {0,0};	// 帧频
    char cTemp = 0;  // 摄氏度+100
    char acAtmosP[2] = {0, 0};    // Pa
    char cHumidty = 0;  // ×100
};

/// ImageStorage数据
struct SImageStorageData
{
    void* pvoidImageStorage = NULL;
    unsigned int uiImageSeq = 0;
    unsigned int uiStatusStore = STATUS_INIT;
    int iStorageStatus = STATUS_INIT;
};

/// ImageReplayer数据
struct SImageReplayerData
{
    void* pvoidImageReplayer = NULL;
    QMutex qmutexReplay;
    SExposureDisplayData sexpdispdataDisp;
    double dExposureTimeDisp = 100;
    int iTriggerModeDisp = TRIGGERMODE_FREE;
    double dFrameFrequencyDisp = 1;
    unsigned short* pausReplayData = NULL;
    int iImageWidth = 6144;
    int iImageHeight = 6144;
    QString qstrTeleID = "1662";
    QString qstrTargetID = "888888";
    QString qstrStartTime = "20210101080800";
};

/// TrackDataStorage数据
struct STrackDataStorageData
{
    void* pvoidTrackDataStorage = NULL;
    QString qstrDateTime = "";
    QString qstrObsID = "";
    unsigned int uiImageSeq = 0;
    unsigned int uiStatusStore = STATUS_INIT;
};

/// 日志
struct SLog
{
    void* pvoidThis = NULL;
};

/// Class GlobalParameter
class GlobalParameter
{
public:
    GlobalParameter();
    ~GlobalParameter();
    static GlobalParameter* GetInstance(void);
    int ReadInitFile(QString qstrFileName);
    int WriteInitFile(void);

public:
    SCommNetSettings m_SCommNetSettings;
    SPath m_SPath;
    SOpticData m_SOpticData;
    SIOTriggerData m_SIOTriggerData;
    SCommExposureData m_SCommExposureData;
    SCommDisplayData m_SCommDisplayData;
    SCommServoData m_SCommServoData;
    SCommMasterControlData m_SCommMasterControlData;
    SCommCameraData m_SCommCameraData;
    SGrabberData m_SGrabberData;
    SImageProcessorData m_SImageProcessorData;
    SDataProcessorData m_SDataProcessorData;
    SImageStorageData m_SImageStorageData;
    SImageReplayerData m_SImageReplayerData;
    STrackDataStorageData m_STrackDataStorageData;
    SNetImageSenderData m_SNetImageSenderData;
    SNetMasterControlData m_SNetMasterControlData;
    SNetErrorDiagnoseData m_SNetErrorDiagnoseData;
    SNetExchangeData m_SNetExchangeData;
    sNetAtmosData m_sNetAtmosData;
    SLog m_SLog;
    SObsParams m_SObsParams;
    bool m_bDebugEN;
    QString m_qstrImageFormat;
};

#endif // GLOBALPARAMETER_H
