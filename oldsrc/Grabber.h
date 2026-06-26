#ifndef GRABBER_H
#define GRABBER_H

#include <QApplication>
#include <QElapsedTimer>
#include <QDateTime>
#include <QTimer>
#include <omp.h>
#include <chrono>
#include <QWaitCondition>
#include <QMutex>

#include "SapClassBasic.h"
#include "StandardThread.h"
#include "GlobalParameter.h"
#include "CommCamera.h"
#include "MyLog.h"
#include "ImageProcessor.h"

typedef struct tagMY_CONTEXT
{
   UINT32   height;
   UINT32   width;
   UINT32   pixDepth;
   void     *image[1];

}MY_CONTEXT, *PMY_CONTEXT;

class Grabber: public QObject
{
    Q_OBJECT

public:
    Grabber();
    ~Grabber();
    void Init(QString ccf);
    unsigned int ResumeGrab(void);
    unsigned int PauseGrab(void);
    void StopGrab(void);
    bool GetWorking(void) { return m_bWorking; }
    bool GetGrabStatus(void) { return m_bGrabbing; }
    unsigned int SetExpTime(QString qstrExposureTime);	// ms
    unsigned int SetGain(QString qstrGain);
    unsigned int SetWindowEN(int line,int start);
    float GetCamHours(void) { return m_fCamWorkingHours; }
    float GetCamTemp(void) { return m_fCamTemp; }
    bool GetWindowEN(void) { return m_bWindowEN; }
    void TrainCam(void);
    void SetFrameInterval(float fFrameInterval);

private:
    static void XferCallback(SapXferCallbackInfo *pInfo);
    bool CreateObjects();
    bool DestroyObjects();
    void GrabImageCopy(void* pausGrabData);
    static void CallBackQuery(void* pvoidThis);	// 处理线程回调
    void GrabSingleFrame(void);
    void StartGrabTimer(void);
    void ClockOn(void);
    int ClockOff(void); // return ms

private slots:
    void on_SignalTimeOut(void);
    void on_SignalTimerCheck(void);
    void on_SignalTimerActiveGrab(void);

signals:
    void SignalGrabData(void);	// 采集到图像信号(ImageProcess处理图像,ImageStorage存储图像)
    void SignalGrabInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight);
    void SignalStartGrab(unsigned int uiGrabWidth, unsigned int uiGrabHeight);

 /// 变量
private:
    SapAcquisition	*m_Acq;
    SapBuffer		*m_Buffers;
    SapTransfer		*m_Xfer;
    unsigned short* m_imgtmp;
    unsigned short* m_out;
    MY_CONTEXT     context;
    UINT32         pixFormat;
    int             pixDepth;
    CommCamera* m_pCameraComm;
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    bool m_bWorking;	// 相机初始化状态
    bool m_bGrabbing;	// 采集标识
    double m_dExposureTime;	// 曝光时间 ms
    bool m_bWindowEN;
    unsigned long long m_ullGrabSeq;
    unsigned long long m_ullGrabSeqCheck;
    float m_fCamWorkingHours;
    float m_fCamTemp;
    QDateTime m_qdtBegin;
    QDateTime m_qdtEnd;
    MyLog* m_pLog;
    QTimer* m_ptimer;
    QTimer* m_ptimerCheck;
    unsigned long m_ulCountQueryTemp;
    bool m_bQueryTemp;
    StandardThread* m_pQueryThread;	// 处理线程对象指针
    static QWaitCondition m_qwcEventQuery;// 处理事件
    static QMutex m_qmEventQuery;
    bool m_bInited;
    QTimer* m_ptimerActiveGrab;
    double m_dTimerInterval;
    std::chrono::high_resolution_clock::time_point m_beginTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
    float m_fFrameInterval;
};

#endif // GRABBER_H
