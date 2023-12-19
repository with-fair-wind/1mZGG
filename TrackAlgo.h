#pragma once
#include <stdio.h>
#include <vector>
#include <set>
#include <time.h>
#include <algorithm>
#include <omp.h>
#include <string.h>
#include <sys/io.h>
#include <sys/sysinfo.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <chrono>
#include <QDebug>
#include "GlobalParameter.h"

#ifndef __linux__
#include "windows.h"
#else
#include "unistd.h"
#include "sys/sysinfo.h"
#endif

#define TRACK_INIT   -1
#define TRACK_GEO    0
#define TRACK_SC     3
#define TRACK_LEO    4
#define TRACK_MANUAL 5

using namespace std;

#define pi 3.141592653589793238463
#define DtR 0.0174532925199432957692369076848861  // 度转化为弧度
#define RtD 57.295779513082320876798154814105170  // 弧度转化为度
#define AStR 4.8481368110953599358991410235794e-6 // 角秒转化为弧度
#define RtAS 2.0626480624709635515647335733077e+5 // 弧度转化为角秒
#define StR 7.2722052166430399038487115353692e-5  // 秒转化为弧度
#define solsid 1.00273790935                      // 太阳时和恒星时比率


static double i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;
static double j00, j11, j2, j3, j4, j5, j6, j7, j8, j9;
static double k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;
static double l0, l1, l2, l3, l4, l5, l6, l7, l8, l9;
static double m0, m1, m2, m3, m4, m5, m6, m7, m8, m9;

#pragma pack(1)
struct ErrorResult
{
    //单位：度
    double ixA = 0;
    double iyA = 0;
    double shuiping = 0;
    double zhaozhun = 0;
    double dingxiang = 0;
    double ixE = 0;
    double iyE = 0;
    double raodong = 0;
    double lingwei = 0;
    double raodong1 = 0;
};
#pragma pack()

struct sOpticParams
{
	int iImageWidth = 6144;	// 图像宽度(pixel)
	int iImageHeight = 6144;	// 图像高度(pixel)
	float fFOVCenterX = 6144 / 2.0;	// 视场中心X(pixel)
	float fFOVCenterY = 6144 / 2.0;	// 视场中心Y(pixel)
	float fPixelScale = 0.0003453;	// 像素比例尺(°/pixel)
};

struct sTime
{
	int iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iMillisecond;
	int iMicrosecond;
};

/// 单帧图像中的单点测量数据(输入数据)
struct sMeasureBlob
{
    unsigned blobID = 0;
    pair<float, float> pairfPos = make_pair(0, 0);	// 连通域质心位置(pixel)
    float fMaxX = 0.0;	// 连通域X方向最大(pixel)
    float fMinX = 0.0;	// 连通域X方向最小(pixel)
    float fMaxY = 0.0;	// 连通域Y方向最大(pixel)
    float fMinY = 0.0;	// 连通域Y方向最小(pixel)
    float fDN = 0.0;	// 连通域灰度和(DN)
    float fArea = 0.0;	// 连通域面积(pixel^2)
    pair<float, float> pairfPosAE = make_pair(0, 0);	// 连通域质心对应的方位俯仰位置(°)

    float fFOVCenterAziModify = 0.0;
    float fFOVCenterEleModify = 0.0;
    float fDistAzi = 0.0;
    float fDistEle = 0.0;
    float fTargetAzi = 0.0;
    float fTargetEle = 0.0;
    double dPointErrEle = 0.0;

    double dAlpha = 0.0;
    double dSigma = 0.0;

    bool bTWDW = false;
    double dRa = 0.0;   //度
    double dDec = 0.0;  //度
    bool bGDCL = false;
    double dDn = 0.0;
    double dGd = 0.0;
};

/// 单帧图像中的测量数据(输入数据)
struct sMeasuresInFrame
{
	sTime stimeFrame;	// 当前帧曝光中心时刻
	unsigned long ulFrameSeq;	// 帧序号
	pair<float, float> pairfFOVCenterAE;	// 视场中心对应的方位、俯仰角(°)
	float fExpTime;	// 曝光时间(s)
	float fFrameFreq;	// 帧频(Hz)
	vector<sMeasureBlob> vectTargetMeasures;	// 单帧图像中的目标位置测量值	
	vector<sMeasureBlob> vectStarMeasures;	// 单帧图像中的恒星位置测量值(可能会包含目标测量值或杂波,但天文定位可以排除干扰)
    double dTemp;
    double dAtmosP;
};

/// 测量结果结构体
struct sResPacket
{
    QString qstrTargetID = "";
    sTime stimeFrame;	// 当前帧曝光中心时刻
    unsigned long ulFrameSeq = 0;	// 帧序号
    float fExpTime = 0;	// 曝光时间(s)
    float fFrameFreq = 0;	// 帧频(Hz)
    sMeasureBlob blob;
    pair<float, float> pairfFOVCenterAE = pair<float, float>(0, 0);	// 视场中心对应的方位、俯仰角(°)
    pair<float, float> pairfFOVCenterAEModify = pair<float, float>(0, 0);	// Modified视场中心对应的方位、俯仰角(°)
    pair<float, float> pairfTargetPosInFrame = pair<float, float>(0,0);
    pair<float, float> pairfTargetDistAE = pair<float, float>(0, 0);
    pair<float, float> pairfTargetPosZXDW = pair<float, float>(0, 0);
    pair<float, float> pairfTargetPosTWDW = pair<float, float>(0,0);     //目标的赤经,赤纬：度
    float fTargetMvGDCL = 0;
    float fTemp = 10.0;    // 溫度/攝氏度
    float fHumidity = 0.3;    // 溼度 0～1
    float fAtmosP = 78900.0;  // 大氣壓 Pa
    bool bValid;
};

/// 单个目标在单帧图像中的信息(输出数据)
struct sTargetInfoInFrame
{
	sTime stimeFrame;	// 当前帧曝光中心时刻
	unsigned long ulFrameSeq;	// 帧序号
	sMeasureBlob blobMeasure;	// 被跟踪的连通域
	pair<float, float> pairfPosZXDW;	// 目标轴系定位位置
	pair<float, float> pairfPosTWDW;	// 目标天文定位位置
	float fMv = 0;	// 目标光度测量值
	bool bValid;	// true:当前帧检测到测量值;false:当前帧未检测到测量值,采用前阵预测值作为此帧后验
};

/// 单个目标在整个搜索或跟踪流程内的信息(输出数据)
struct sTargetInfo
{
    QString qstrTargetID = "";
    QString qstrSaveStartTime = "";
    QString qstrFileNameGAE = "";
	vector<sTargetInfoInFrame> vectInfoInFrame;	// 目标帧内信息队列
    vector<sResPacket> vectResPacket;   // 测量结果队列
	pair<float, float> pairfPredPosInFrame;	// 目标帧内预测位置,量纲为"pixel"
	pair<float, float> pairfPredPosAE;	// 目标结合望远镜方位、俯仰角预测位置,量纲为"°"
	pair<float, float> pairfPredSpdInFrame;	// 目标帧内预测速度,量纲为"pixel/frame"
	pair<float, float> pairfPredSpdAE;	// 目标结合望远镜方位、俯仰角预测速度,量纲为"°/s"

    pair<float, float> pairPredSpdRaDec;
    pair<float, float> pairPredPosRaDec;

    float fValid = 1.0;	// 目标存在性分数(0~1.0,无测量占比)
    bool bLiving = false;	// 目标是否存活
    vector<unsigned long> vectulFrameSeqTWDW;
    vector<unsigned long> vectulFrameSeqStore;
};

/// GEO模式的设置参数
struct sTrackingSettings
{
	/// GEO/GEO_AE/MEO/LEO共用参数
	sOpticParams opticparams;	// 光学参数
	float fThreashLiving = 0.5;	// 目标存活率判据,0~1,推荐0.5
	int iNumFramesLiving = 10;	// 目标存活判断时已关联的帧数判据,推荐10
	/// GEO/GEO_AE/MEO共用参数
	float fRadius = 50;	// fRadius为搜索波门半径,单位pixel,推荐50
	/// GEO/GEO_AE共用参数
	float fRatioFOV = 0.25;	// fRatioFOV为恒星检测FOV比例,推荐0.25	
	float fThreashStarMode = 10;	// fThreashStarMode为恒动模式的确认阈值,单位pixel,推荐10
	float fThreashGazeMode = 2;	// fThreashGazeMode为凝视模式的确认阈值,单位pixel,推荐2
	/// MEO/LEO共用参数
	bool bAutoDecide = true;	// 跟踪过程中是否自动判断目标存活	
	/// MEO参数
	float fThreashMEO = 5;	// 低帧频下的MEO/LEO跟踪确认阈值,单位pixel,推荐5
	/// LEO参数
	float fSpdLow_AE;	// fSpdLow_AE为搜索速度低阈值,单位°(间隔为图像采集周期),需大于恒星速度,如20×opticparams.fPixelScale
	float fSpdHigh_AE;	// fSpdHigh_AE为搜索速度高阈值,单位°(间隔为图像采集周期),需小于目标最大速度,如200×opticparams.fPixelScale
	float fThreash_AE;	// fThreash_AE为速度确认阈值,单位°(间隔为图像采集周期),推荐2×opticparams.fPixelScale;	
};



class TrackAlgo
{
	/// 函数
public:
	TrackAlgo();
	~TrackAlgo();
	/// 跟踪器初始化
    int InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID);
	/// 计算恒星速度
	int CalcStarSpd(sMeasuresInFrame measures, pair<float, float>& pairfSpd, pair<float, float>& pairfSpd_AE, int& iNumMost);
	/// 跟踪器的检测跟踪过程,循环压入测量数据
    int TrackProc_RaDec(sMeasuresInFrame measures, vector<sTargetInfo>& vectTargetInfo, bool bFullLEO);	// 必须在CalcStarSpd()后调用
    int TrackProc_GEO(sMeasuresInFrame measures, vector<sTargetInfo>& vectTargetInfo, bool bFullLEO, vector<pair<QString, sMeasureBlob>> vecGEOisbValidBlob);	// 必须在CalcStarSpd()后调用
	int TrackProc_LEO(sMeasuresInFrame measures, sTargetInfo& targetInfo);	
    int TrackProc_SC(sMeasuresInFrame measures, sTargetInfo& targetInfo);
    int TrackProc_MANUAL(sMeasuresInFrame measures, sTargetInfo& targetInfo, const sMeasureBlob& ManualBlob);
	/// 杀死当前跟踪目标
	int KillTarget(void);
	/// 获取目标数据队列,适用于GEO/GEO_AE两种模式
	int GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo);
	/// 获取目标数据,适用于LEO模式
	int GetTargetInfo(sTargetInfo& targetInfo);
	/// 获取目标跟踪检测状态
	bool GetTargetFind(void) { return m_bTargetFind; }
	/// 获取目标跟踪确认状态
	bool GetTargetVerify(void) { return m_bTargetVerify; }

    void BJ2UTC(int iBJYear, int iBJMonth, int iBJDay, int iBJHour, int &iUTCYear, int &iUTCMonth, int &iUTCDay, int &iUTCHour);
    void AangleToEquator(double A, double E, double Longm, double phim, int y, int m, int d, int hour, int min, double sec, double p, double T, bool nflag, double *Ra, double *De, double *Rm, double *Dm, bool zflag);
    void setRaDecThresh(const double& dRaThresh, const double& dDecThresh, const double& dRaSpdThresh, const double& dDecSpdThresh){m_dRaThresh = dRaThresh; m_dDecThresh = dDecThresh;
                                                                                                                                   m_dRaSpdThresh = dRaSpdThresh; m_dDecSpdThresh = dDecSpdThresh;}
    int getExponent(double num);

private:
	void BubbleSortReverse(vector<int>& vectInput);
	void FindMost(vector<pair<int, int>> vectInput, pair<int, int>& pairMost, int& iNumMost);
	int CalcStarSpd(vector<sMeasuresInFrame> vectMeasuresInputFIFO, float fRatioFOV, float fRadius, float fThresh, sOpticParams opticparams, pair<float, float>& pairfSpd, pair<float, float>& pairfSpd_AE, int& iNumMost);
    int RaDec_Assoc4(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo);
    int RaDec_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo);
    int RaDec_ReFindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo);
    int RaDec_TrackTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fThresh, sOpticParams opticparams);
    int GEO_Assoc4(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams);
	int GEO_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams);
	int GEO_ReFindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThreshd, sOpticParams opticparams);
    int GEO_TrackTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, vector<pair<QString, sMeasureBlob>> vecGEOisbValidBlob);
    int SC_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams);
    int SC_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams);
    int SC_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving);
    int SC_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide);
    int MANUAL_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams);
    int MANUAL_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams, const sMeasureBlob& ManualBlob);
    int MANUAL_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, const sMeasureBlob& ManualBlob);
    int MANUAL_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide, const sMeasureBlob& ManualBlob);
    int LEO_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fSpdLow_AE, float fSpdHigh_AE, float fThresh_AE, sOpticParams opticparams);
	int LEO_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fSpdLow_AE, float fSpdHigh_AE, float fThresh_AE, sOpticParams opticparams);
	int LEO_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh_AE, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving);
	int LEO_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh_AE, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide);
	int InitTargetInfo(void);
	int InitFIFO_Target(void);
	int InitFIFO_Star(void);
    void CalcDiffTime(sTime beginTime, sTime endTime, double& dDiffSec);
    float MedianFloat3(float a, float b, float c);
    float MedianFloat5(float a, float b, float c, float d, float e);

    double JDE(int Y, int M, int D, int hour, int min, double sec); // 计算简约儒略日，输入世界时，J2000
    double Nut1(double t0);                                         // 求黄经章动，输入简约儒略日
    double Nut2(double t0);                                         // 求倾角章动函数，输入简约儒略日

    /// 变量
private:
    GlobalParameter *m_pGParam;
	int m_iNumCPUCores;
	int m_iMode;
	sTrackingSettings m_settings;
	vector<sMeasuresInFrame> m_vectFIFO_Target;	// 用于目标检测与跟踪
	vector<sMeasuresInFrame> m_vectFIFO_Star;	// 用于计算恒星速度	
    vector<sTargetInfo> m_vectTargetInfo;	// GEO/GEO_AE/MEO搜索/跟踪目标数据队列
    vector<sTargetInfo> m_vectTargetInfoFind;	// LEO检测疑似目标数据队列
    sTargetInfo m_targetInfo;	// LEO跟踪目标数据队列
	bool m_bTargetFind;
	bool m_bTargetVerify;
	unsigned long m_ulFrameSeq;
	pair<float, float> m_pairfStarSpd;
	pair<float, float> m_pairfStarSpd_AE;
    float m_fFrameFreq;
    int m_iTargetID;
    bool m_bFullLEO;

    double m_dRaThresh;
    double m_dDecThresh;
    double m_dRaSpdThresh;
    double m_dDecSpdThresh;

    vector<sMeasureBlob> m_vectManualAddTop3blob;
};







