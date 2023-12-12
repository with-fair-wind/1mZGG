#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H


#include <math.h>
#include <algorithm>
#include <QWaitCondition>
#include <QMutex>
#include <QDebug>
#include <QDateTime>
#include "StandardThread.h"
#include "GlobalParameter.h"
#include "ImageProcAlgo.h"

class GPUImageAccelerator;

class ImageProcessor : public QObject
{
Q_OBJECT

/// 函数
public:
    ImageProcessor(void);
    ~ImageProcessor(void);
    int InitProc(int iProcMode, int iTrackMode, size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate, sTrackingSettings trackSettings);
    void SetCenterMode(bool bCenterMode);
    void SetBlobAreaLimit(int iMinArea, int iMaxArea);
    void SetScale(bool bAutoScale, unsigned int uiScaleMax, unsigned int uiScaleMin);
    void SetDispEnhance(bool bDispEnhance);
    void SetDispBW(bool bDispBW);
    void GetFramePacket(vector<sFramePacket>& vectFramePacket);
    int GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo);
    int GetTargetInfo(sTargetInfo& targetInfo);
    unsigned char* GetDispImage(unsigned int& uiWidth, unsigned int& uiHeight);
    int GetTrackMode(void)  {return m_iTrackMode;}
    int GetProcMode(void)   {return m_iProcMode;}
    unsigned int GetNumFrames(void)  {return m_uiFrameSeq;} // m_uiFrameSeq++
    bool GetProcessMode(void)   {return m_bProcessMode;}
    int InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID);
    void SetTrackProc(bool bTrackProc) { m_pImageProc->SetTrackProc(bTrackProc); }
    bool GetTrackProc(void) { return m_pImageProc->GetTrackProc(); }
    unsigned short* GetFlatImage(unsigned int uiImageWidth, unsigned int uiImageHeight) { return m_pImageProc->GetFlatImage(uiImageWidth, uiImageHeight); }
    unsigned short* GetBiasImage(unsigned int uiImageWidth, unsigned int uiImageHeight) { return m_pImageProc->GetBiasImage(uiImageWidth, uiImageHeight); }
    unsigned int GetImageWidth(void) { return m_uiGrabWidth; }
    unsigned int GetImageHeight(void) { return m_uiGrabHeight; }
    void SetDark(bool bDark) { m_pImageProc->SetDark(bDark); }
    void SetStdRatio(float fRatio) { m_pImageProc->SetStdRatio(fRatio); }
    void Init_TWDW(){m_pImageProc->Init_TWDW();}
    void Init_Photometry(){m_pImageProc->InitPhotometry();}
    void Set_Photometry(){m_pImageProc->SetPhotometry();}
    void GetVectTargetTWDWInfo(vector<sTargetInfo>& vectTargetInfo){m_pImageProc->GetVectTargetTWDWInfo(vectTargetInfo);}
    void BJ2UTC(int iBJYear, int iBJMonth, int iBJDay, int iBJHour, int &iUTCYear, int &iUTCMonth, int &iUTCDay, int &iUTCHour){m_pImageProc->BJ2UTC(iBJYear, iBJMonth, iBJDay, iBJHour, iUTCYear, iUTCMonth, iUTCDay, iUTCHour);}
    void CalcPointError(double dAzi, double dEle, double& dAziErr, double& dEleErr){m_pImageProc->CalcPointError(dAzi, dEle, dAziErr, dEleErr);}
    CStarMap* GetStarMap() {return m_pImageProc->GetStarMap();}
    void CalcDistortionDelta(double dPosX, double dPosY, double &dPosDx, double &dPosDy){m_pImageProc->CalcDistortionDelta(dPosX, dPosY, dPosDx, dPosDy);}
    void setRaDecThresh(const double& dRaThresh, const double& dDecThresh, const double& dRaSpdThresh, const double& dDecSpdThresh) {m_pImageProc->setRaDecThresh(dRaThresh, dDecThresh, dRaSpdThresh, dDecSpdThresh);}

private:
    static void CallBackProcess(void* pvoidThis);	// 处理线程回调
    void MarkDispInfor(void);	// 标记显示信息

signals:
    void SignalDisplay(void); // 显示信号
    void SignalSend(void);  // 送显信号
    void SignalTrackData(void);	// 提取到跟踪数据信号
    void SignalGrabDataRotate(void);

private slots :
    void on_SignalGrabInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight);    // 开启图像采集后,Grabber会发出此信号,用于InitProc(PROC_NONE,...)
    void on_SignalReplayInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight); // 开启图像采集后,Grabber会发出此信号,用于InitProc(PROC_NONE,...)
    void on_SignalGrabData(void);
    void on_SignalReplayData(void);

public slots:
    void on_SignalDispStatus();

/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    StandardThread* m_pProcessThread;	// 处理线程对象指针
    static QWaitCondition m_qwcEventProcess;// 处理事件
    static QMutex m_qmEventProcess;
    ImageProcAlgo* m_pImageProc;
    unsigned int m_uiGrabWidth;
    unsigned int m_uiGrabHeight;
    unsigned int m_uiCropWidth;
    unsigned int m_uiCropHeight;
    unsigned int m_uiDispWidth;
    unsigned int m_uiDispHeight;
    unsigned int m_uiFrameSeq;
    bool m_bProcessMode;	// 处理模式 true:采集数据处理 false:回放数据处理
    int m_iProcMode;	// 处理模式,详见ImageProcAlgo.h宏定义
    int m_iTrackMode;
    bool m_bCenterMode;	// 提取形心或质心,true:形心,false:质心
    bool m_bAutoScale;  // 自动拉伸标识
    unsigned int m_uiScaleMax;    // 拉伸最大值
    unsigned int m_uiScaleMin;    // 拉伸最小值
    int m_iMaxArea;	// 最大连通域面积
    int m_iMinArea;	// 最小连通域面积
    bool m_bInitTrack;
    sTrackingSettings m_settings;
    int m_iTargetID;
};

#endif // IMAGEPROCESSOR_H
