#ifndef IMAGEPROCALGO_H
#define IMAGEPROCALGO_H

#include <emmintrin.h>
#include <math.h>
#include <vector>
#include <chrono>
#include <CL/opencl.h>
#include <stdio.h>
#include "DeviceManager.h"
#include "TrackAlgo.h"
#include <omp.h>
#include "NetExchange.h"
#include "Labeler.h"
#include "GlobalParameter.h"
#include "photometry/dllphotometry.h"
#include "starmap/starmap.h"
#include <QMap>

#define  PROC_NONE		0	// 不做目标提取
#define  PROC_DIFF		1	// 帧差目标提取	
#define  PROC_DIRECT	3	// 直接目标提取

using namespace std;

struct sImageData
{
	double dMax = 0;	// 图像最大灰度值
	double dMin = 0;	// 图像最小灰度值
	double dAvg = 0;	// 图像灰度均值
	double dStd = 0;	// 图像灰度标准差
	void* pvoidBuffer = NULL;	// 图像指针
};

class sTWDWfixData
{
public:
    bool bisTrue = false;
    double (*dblStarShow)[9] = nullptr;
    double (*dblStarCrop)[9] = nullptr;
    int             nStarShow=0;
    int             nStarCrop=0;
    int nYear = 0;
    int nMonth = 0;
    int nDay = 0;
    int nHour = 0;
    int nMinute = 0;
    double dblHour = 0;
    double dblPointA = 0;
    double dblPointE = 0;
    double dblwendu = 0;
    double dbldqy = 0;
    double dblshidu = 0;
    double dblSecond = 0;
    QPair<int , QMap<QString, double>> qPairMatchErrorRes;
    sTWDWfixData()
    {
        dblStarShow = new double[25002][9];
        dblStarCrop = new double[25002][9];
    }
    sTWDWfixData(const sTWDWfixData& other)
    {
        bisTrue = other.bisTrue;
        dblStarShow = new double[25002][9];
        dblStarCrop = new double[25002][9];
        memcpy(dblStarShow, other.dblStarShow, 25002 * 9 * sizeof (double));
        memcpy(dblStarCrop, other.dblStarCrop, 25002 * 9 * sizeof (double));
        nStarShow = other.nStarShow;
        nStarCrop = other.nStarCrop;
        nYear = other.nYear;
        nMonth = other.nMonth;
        nDay = other.nDay;
        nHour = other.nHour;
        nMinute = other.nMinute;
        dblHour = other.dblHour;
        dblPointA = other.dblPointA;
        dblPointE = other.dblPointE;
        dblwendu = other.dblwendu;
        dbldqy = other.dbldqy;
        dblshidu = other.dblshidu;
        dblSecond = other.dblSecond;
        qPairMatchErrorRes = other.qPairMatchErrorRes;
    }
    sTWDWfixData& operator=(const sTWDWfixData& other)
    {
        if(this == &other)
            return *this;
        bisTrue = other.bisTrue;
        memcpy(dblStarShow, other.dblStarShow, 25002 * 9 * sizeof (double));
        memcpy(dblStarCrop, other.dblStarCrop, 25002 * 9 * sizeof (double));
        nStarShow = other.nStarShow;
        nStarCrop = other.nStarCrop;
        nYear = other.nYear;
        nMonth = other.nMonth;
        nDay = other.nDay;
        nHour = other.nHour;
        nMinute = other.nMinute;
        dblHour = other.dblHour;
        dblPointA = other.dblPointA;
        dblPointE = other.dblPointE;
        dblwendu = other.dblwendu;
        dbldqy = other.dbldqy;
        dblshidu = other.dblshidu;
        dblSecond = other.dblSecond;
        qPairMatchErrorRes = other.qPairMatchErrorRes;
        return *this;
    }
    ~sTWDWfixData()
    {
        if(dblStarShow)
        {
            delete [] dblStarShow;
            dblStarShow = nullptr;
        }
        if(dblStarCrop)
        {
            delete [] dblStarCrop;
            dblStarCrop = nullptr;
        }
    }
};

struct sFramePacket
{
	sTime stimeFrame;	// 当前帧曝光中心时刻
	unsigned long ulFrameSeq = 0;	// 帧序号
	pair<float, float> pairfFOVCenterAE = pair<float, float>(0, 0);	// 视场中心对应的方位、俯仰角(°)
    pair<float, float> pairfFOVCenterAEModify = pair<float, float>(0, 0);	// Modified视场中心对应的方位、俯仰角(°)
    float fExpTime = 0;	// 曝光时间(ms)
	float fFrameFreq = 0;	// 帧频(Hz)
	sImageData sausImageGrab;	// 采集图像数据
	sImageData sausImageEnhance;	// 增强图像数据
    sImageData safImagePhotometry; // 光度测量图像
	vector<sMeasureBlob> vectStarMeasures;	// 单帧图像中的恒星位置测量值(用于恒星速度计算与天文定位,可能包含杂波)
	vector<sMeasureBlob> vectTargetMeasures;	// 单帧图像中的目标位置测量值(用于目标检测与跟踪,可能包含杂波)
    float fTemp;
    float fAtmosP;
    float fHumidity;
    sTWDWfixData stwdwFixData;
};

struct sPacketGAE
{
    QString qstrStorePath;
    QString qstrFileNameGAE;
    QString qstrTaskID;
    QString qstrObsID;
    QString qstrStartTime;
    QString qstrEndTime;
    char strExpTime[300] = "";  // Compare to GTW
    char strDWDataNew[300] = "";
};

class ImageProcAlgo
{
	/// 函数
public:
	ImageProcAlgo();
	virtual ~ImageProcAlgo();
	DeviceManager* GetGPUDeviceManager() { return m_pGpuDevMngr; }	
    int InitProc(int iProcMode, int iTrackMode, size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate, sTrackingSettings trackSettings);
	int InputFramePacket(sFramePacket sPacket);
    int ProcFramePacket(void);
	void SetBlobAreaLimit(int iMinArea, int iMaxArea);
	void SetCenterMode(bool bCenterMode) { m_bCenterMode = bCenterMode; }
	void SetDispEnhance(bool bDispEnhance) { m_bDispEnhance = bDispEnhance; }
    void SetDispBW(bool bDispBW)  { m_bDispBW = bDispBW; }
	void SetScale(bool bAutoScale, unsigned int uiScaleMax, unsigned int uiScaleMin) {
		m_bAutoScale = bAutoScale;
		m_uiScaleMax = uiScaleMax;
		m_uiScaleMin = uiScaleMin;
	}
	void SetProcImageSave(bool bSave) { m_bProcImageSave = bSave; }	
	void GetFramePacket(vector<sFramePacket>& vectFramePacket) { vectFramePacket = m_vectFramePacket; }	
	int GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo);
    int GetVectTargetTWDWInfo(vector<sTargetInfo>& vectTargetInfo){vectTargetInfo = m_vectTargetInfo;}
	int GetTargetInfo(sTargetInfo& targetInfo);
	unsigned char* GetDispImage(void) { return m_paucDisp; }	
    int InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID);
    void SetTrackProc(bool bTrackProc) { m_bTrackProc = bTrackProc; }
    bool GetTrackProc(void) { return m_bTrackProc; }
    unsigned short* GetFlatImage(unsigned int uiImageWidth, unsigned int uiImageHeight)  {
        if (uiImageWidth == m_pGParam->m_SGrabberData.iFullWidth && uiImageHeight == m_pGParam->m_SGrabberData.iFullHeight)  {
            return m_pausFlatFull;
        }
        else if (uiImageWidth == m_pGParam->m_SGrabberData.iSubWidth && uiImageHeight == m_pGParam->m_SGrabberData.iSubHeight)  {
            return m_pausFlatCrop;
        }
        return NULL;
    }
    unsigned short* GetBiasImage(unsigned int uiImageWidth, unsigned int uiImageHeight)  {
        if (uiImageWidth == m_pGParam->m_SGrabberData.iFullWidth && uiImageHeight == m_pGParam->m_SGrabberData.iFullHeight)  {
            return m_pausBiasFull;
        }
        else if (uiImageWidth == m_pGParam->m_SGrabberData.iSubWidth && uiImageHeight == m_pGParam->m_SGrabberData.iSubHeight)  {
            return m_pausBiasCrop;
        }
        return NULL;
    }
    void SetDark(bool bDark) {
        m_bDark = bDark;
        if (!bDark)
            m_fRatioStd = 2.5;
    }
    void SetStdRatio(float fRatio) {
        m_fRatioStd = fRatio;
        m_pGParam->m_SImageProcessorData.fRatioStd = m_fRatioStd;
    }
    void ClockOn(void);
    int ClockOff(void); // return ms
    void InitData();
    void Init_TWDW();
    void InitPhotometry(void);
    void SetPhotometry(void);
    void BJ2UTC(int iBJYear, int iBJMonth, int iBJDay, int iBJHour, int &iUTCYear, int &iUTCMonth, int &iUTCDay, int &iUTCHour)
    {m_pTrakcer->BJ2UTC(iBJYear, iBJMonth, iBJDay, iBJHour, iUTCYear, iUTCMonth, iUTCDay, iUTCHour);}
    void CalcPointError(double dAzi, double dEle, double& dAziErr, double& dEleErr);
    CStarMap* GetStarMap() {return m_pstarmap;}
    void CalcDistortionDelta(double dPosX, double dPosY, double &dPosDx, double &dPosDy);
    void setRaDecThresh(const double& dRaThresh, const double& dDecThresh, const double& dRaSpdThresh, const double& dDecSpdThresh) {m_pTrakcer->setRaDecThresh(dRaThresh, dDecThresh, dRaSpdThresh, dDecSpdThresh);}

private:
    int InitGPU(size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate);
    int InitCLKernel(void);
    int Proc_Rotate(void);
	int Proc_None(void);
	int Proc_Diff(void);
	int Proc_Direct(void);
	int AvgStdGPU16(cl_mem cmInput, double* pdAvg, double* pdStdev, size_t szImageWidth, size_t szImageHeight);
	int AvgStdGPU8(cl_mem cmInput, double* pdAvg, double* pdStdev, size_t szImageWidth, size_t szImageHeight);
	int MinMaxGPU16(cl_mem cmInput, double* pdMin, double* pdMax, size_t szImageWidth, size_t szImageHeight);
	int MinMaxGPU8(cl_mem cmInput, double* pdMin, double* pdMax, size_t szImageWidth, size_t szImageHeight);
	int Short2ByteAutoGPU(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int* minv, int* maxv, double minscale = 1, double maxscale = 1, double lim = -1, double thresh = -1);	
	int Short2ByteMinMaxGPU(cl_mem cmInput, cl_mem cmOutput, int minv, int maxv, size_t szImageWidth, size_t szImageHeight);
    int SubBG16(cl_mem cmInput, cl_mem cmOutput, float fSigma2d, float fAvr, float fStd, float fRatioStd, size_t szImageWidth, size_t szImageHeight);
	int Crop16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, size_t szCropWidth, size_t szCropHeight);
	int Binary16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, unsigned short usThresh);
	int SubFrameNoStar(cl_mem cmInput1, cl_mem cmInput2, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize, int iErodeSize, pair<float, float> pairfStarSpd);
	int MedianFilter16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iHSize);
	int SaltFilter8(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight);
	int Binning16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iBinning);
	int Median5FramesGPU(cl_mem cmIn1, cl_mem cmIn2, cl_mem cmIn3, cl_mem cmIn4, cl_mem cmIn5, cl_mem cmOut, size_t szImageWidth, size_t szImageHeight);
    int Rotate16(cl_mem cmIn, cl_mem cmOut1, cl_mem cmOut2, size_t szImageWidth, size_t szImageHeight, bool bRotate, double dAvg, double dStd, bool bNoSatuZone);
	void CropPos2RealPos(pair<float, float> pairfCropPos, pair<float, float>& pairfRealPos, size_t szImageWidth, size_t szImageHeight, size_t szCropWidth, size_t szCropHeight);
    void LoadBadPixel(QString qstrFileName);
    int RemoveBadLine(cl_mem cmIn, cl_mem cmOut, size_t szImageWidth, size_t szImageHeight);
    void RemoveBadPixel(Blobs& blobs);
    void RemoveBadBlob(Blobs& blobs);
    int DilateGray16(cl_mem cmInput1, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize);
    int DilateGray8(cl_mem cmInput1, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize);
	size_t DivUp(size_t dividend, size_t divisor);
	size_t RoundUp(int group_size, int global_size);
    bool ReadFlatImage(QString qstrFileName);
    bool ReadBiasImage(QString qstrFileName);
    int FlatBiasCorrect(cl_mem cmInput, cl_mem cmOutput, cl_mem cmFlat, cl_mem cmBias, double dFlatAvr, size_t szImageWidth, size_t szImageHeight);
    bool CalcTargetInSubZone(unsigned short* pausInput, pair<float,float> pairfFOVCenterAE, pair<float,float> pairfSelectPos, sMeasureBlob& blob, float fThresh1, float fThresh2);
    int LocalContrastMeasure(cl_mem cmInput, cl_mem cmOutput, int iSize, size_t szImageWidth, size_t szImageHeight);
    int ReduceLargeDN16(cl_mem cmInput, cl_mem cmOutput, float fThresh, size_t szImageWidth, size_t szImageHeight);
    int ReduceSmallDN16(cl_mem cmInput, cl_mem cmOutput, float fAvr, float fStd, size_t szImageWidth, size_t szImageHeight);
    int Patch16(cl_mem cmInput, cl_mem cmOutput, int r, int c);
    int DePatch8(cl_mem cmInput, cl_mem cmOutput, int r, int c);
    int DePatch16(cl_mem cmInput, cl_mem cmOutput, int r, int c);    
    double RefractVisual(const double vAlt, const double pressure, const double temperature);
    void TimeAddSeconds(int iYear, int iMonth, int iDay, int iHour, int iMinute, double dSecond, double dSecondAdd, int &iYearNew, int &iMonthNew, int &iDayNew, int &iHourNew, int &iMinuteNew, double &dSecondNew);
    void LoadStarLib_TWDW();
    void ChangeGAEStr(char *pacOldStr, char *pacNewStr, char* pacExpStr);
    void WriteVedioTagFile(QString qstrPathName, QString qstrFileName, sMeasureBlob blob, QString qstrTargetID);    
    void ChangeGTWStr(char *pacOldStr, char *pacNewStr, char* pacExpStr);
    void ChangeGDJStr(char *pacOldStr, char *pacNewStr);
    void WriteFile(QString qstrPathName, QString qstrFileName, QString qstrTaskID, QString qstrObsID, QString qstrStartTime, QString qstrEndTime, char* paucInfo);
    void WritePasFile(QString qstrPathName, QString qstrFileName, char* pacInfo);
    void ChangeGAEStrMv(char *pacOldStr, char *pacNewStr, float fMv);
    void TWDataDeal(unsigned long long ullFrameID);
    void ProcessTWDW_GDCL(vector<int> vecTarget, unsigned long long ullFrameID);
    void GetFirstResPackTime(const char *strSrc, QString &res);
    bool ReadPointError(QString qstrFileName);
    void ReadJBCoef(QString qstrFile, double *padJB_XCoef, double *padJB_YCoef);
    int ErodeBW(cl_mem cmInput, cl_mem cmOutput, int iHSize, size_t szImageWidth, size_t szImageHeight);
    int ErodeGray(cl_mem cmInput, cl_mem cmOutput, int iHSize, size_t szImageWidth, size_t szImageHeight);


	/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
	/// 跟踪参数
	TrackAlgo* m_pTrakcer;	// 跟踪器
	vector<sTargetInfo> m_vectTargetInfo;	// GEO/GEO_AE搜索/跟踪目标数据队列(交由TrackAlgo维护)
	sTargetInfo m_targetInfo;	// LEO跟踪目标数据队列(交由TrackAlgo维护)
	sTrackingSettings m_trackSettings;	// 光学参数
	/// OpenCL参数
	DeviceManager* m_pGpuDevMngr;	// GPU设备管理器
	size_t m_szGrabWidth;	// 采集图像宽度
	size_t m_szGrabHeight;	// 采集图像高度
    size_t m_szImageWidth;	// 图像宽度
	size_t m_szImageHeight;	// 图像高度  	
	size_t m_szCropWidth;	// 中心裁剪图像高度
	size_t m_szCropHeight;	// 中心裁剪图像宽度
	int m_iBinning;	// 合并像素
	cl_kernel m_ckHist16;	// OpenCL内核函数-16位图像的直方图计算
	cl_kernel m_ckShort2Byte;	// OpenCL内核函数-16位图像转8位图像
	cl_kernel m_ckAvgGroupSum16;	// OpenCL内核函数-16位图像求均值GroupSum
	cl_kernel m_ckVarGroupSum16;	// OpenCL内核函数-16位图像求方差GroupSum
	cl_kernel m_ckAvgGroupSum8;	// OpenCL内核函数-8位图像求均值GroupSum
	cl_kernel m_ckVarGroupSum8;	// OpenCL内核函数-8位图像求方差GroupSum
	cl_kernel m_ckDoubleGlobalSum;	// OpenCL内核函数-double型显存GlobalSum
	cl_kernel m_ckMinMaxGroup16;	// OpenCL内核函数-16位图像求最小值最大值Group
	cl_kernel m_ckMinMaxGroup8;	// OpenCL内核函数-8位图像求最小值最大值Group
	cl_kernel m_ckDoubleGlobalMinMax;	// OpenCL内核函数-double型显存GloabalMinMax
	cl_kernel m_ckGaussianFilter16;	// OpenCL内核函数-16位图像高斯滤波
	cl_kernel m_ckGaussianFilter8;	// OpenCL内核函数-8位图像高斯滤波
	cl_kernel m_ckReduceSaturation16;	// OpenCL内核函数-16位图像去饱和
	cl_kernel m_ckReduceSaturation8;	// OpenCL内核函数-8位图像去饱和	
	cl_kernel m_ckSubFrame16;	// OpenCL内核函数-16位图像帧差
	cl_kernel m_ckSubFrame8;	// OpenCL内核函数-8位图像帧差
	cl_kernel m_ckCrop16;	// OpenCL内核函数-16位图像裁剪
	cl_kernel m_ckBinary16;	// OpenCL内核函数-16位图像二值化
	cl_kernel m_ckGrayDilate16;	// OpenCL内核函数-16位图像灰度膨胀
    cl_kernel m_ckGrayDilate8;
	cl_kernel m_ckGrayErode16;	// OpenCL内核函数-16位图像灰度腐蚀
    cl_kernel m_ckBWErode8;
	cl_kernel m_ckMedianFilter16;	// OpenCL内核函数-16位图像中值滤波
	cl_kernel m_ckSaltFilter8;	// OpenCL内核函数-8位图像椒盐滤波
	cl_kernel m_ckBinning16;	// OpenCL内核函数-16位图像Binning
	cl_kernel m_ckFrame5Median16;	// OpenCL内核函数-16位图像5帧中值滤波
    cl_kernel m_ck_Rotate16;
    cl_kernel m_ckRemoveBadLine;
    cl_kernel m_ckFlatBiasCorrect;
    cl_kernel m_ckLCM;
    cl_kernel m_ckReduceLargeDN16;
    cl_kernel m_ckReduceSmallDN16;
    cl_kernel m_ckPatch16;
    cl_kernel m_ckDePatch8;
    cl_kernel m_ckDePatch16;
	cl_mem m_cmGrab;	// 显存-采集图像
    cl_mem m_cmOri;	// 显存-原始图像
    cl_mem m_cmRemoveBadLine;
    cl_mem m_cmRotate;
    cl_mem m_cmPhotometry;
    cl_mem m_cmMedian;
	cl_mem m_cmEnhance;	// 显存-增强图像
    cl_mem m_cmEnhanceMedian;
	cl_mem m_cmEnhancePre;	// 显存-前帧增强图像
	cl_mem m_cmCrop;	// 显存-裁剪图像
	cl_mem m_cmCropBW;	// 显存-裁剪二值化图像
	cl_mem m_cmSubDilate;	// 显存-帧差灰度膨胀图像后的图像
	cl_mem m_cmNoStar;	// 显存-无恒星图像	
	cl_mem m_cmNoStarBW;	// 显存-无恒星二值化图像
	cl_mem m_cmNoStarBW2;	// 显存-无恒星椒盐滤波二值化图像
	cl_mem m_cmDisp;	// 显存-显示图像
	cl_mem m_cmBg1;	// 显存-高斯滤波背景1
	cl_mem m_cmBg2;	// 显存-高斯滤波背景2
	cl_mem m_cmDilate;	// 显存-膨胀图像
	cl_mem m_cmEnhance1;	// 显存-增强图像1
	cl_mem m_cmEnhance2;	// 显存-增强图像2
	cl_mem m_cmEnhance3;	// 显存-增强图像3
	cl_mem m_cmEnhance4;	// 显存-增强图像4
	cl_mem m_cmEnhance5;	// 显存-增强图像5
    cl_mem m_cmFlat;
    cl_mem m_cmFlatCrop;
    cl_mem m_cmBias;
    cl_mem m_cmBiasCrop;
    cl_mem m_cmCorrect;
    cl_mem m_cmPitchEnhance;
    cl_mem m_cmPitchEnhance1;
    cl_mem m_cmPitchEnhance2;
    cl_mem m_cmPitchEnhance3;
    cl_mem m_cmPitchEnhance4;
    cl_mem m_cmPitchEnhance5;
    cl_mem m_cmPitchNoStarBW;
	/// 连通域参数
	Labeler* m_pLabeler;	// 连通域计算
	Blobs m_blobs;	// 连通域计算结果
	int m_iMaxArea;	// 最大连通域面积
	int m_iMinArea;	// 最小连通域面积
	/// 其它参数
	vector<sFramePacket> m_vectFramePacket;	// 帧数据包队列	
	int m_iProcMode;	// 处理模式,详见ImageProcAlgo.h宏定义
	int m_iTrackMode;	// 跟踪模式,详见TrackAlgo.h宏定义
	unsigned long m_ulFrameSeq;	// 单次处理序列图像序号
	bool m_bDispEnhance;	// 显示图像是否增强	
	unsigned char* m_paucCropBW;	// 裁剪二值化图像
	unsigned char* m_paucNoStarBW;	// 无恒星二值化图像
	unsigned char* m_paucDisp;	// 显示图像
	bool m_bInit;	// 初始化成功标识
	bool m_bProcImageSave;	// 保存中间图像
	bool m_bCenterMode;	// 提取形心或质心,true:形心,false:质心
	bool m_bAutoScale;  // 自动拉伸标识
	unsigned int m_uiScaleMax;    // 拉伸最大值
	unsigned int m_uiScaleMin;    // 拉伸最小值
    bool m_bRotate;
    bool m_bNoSatuZone;
    bool m_bTrackProc;
    vector<pair<float,float>> m_vectpairfBadPixel;
    int m_iNumBlob;
    float m_fRatioStd;
    unsigned short* m_pausFlatFull;
    unsigned short* m_pausFlatCrop;
    unsigned short* m_pausBiasFull;
    unsigned short* m_pausBiasCrop;
    double m_dFlatAvrFull;
    double m_dFlatAvrCrop;
    bool m_bCLKernelInited;
    std::chrono::high_resolution_clock::time_point m_beginTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
    bool m_bDark;
    float m_fExpTime;
    int m_iNumMost;
    float m_fAvr;
    float m_fStd;
    bool m_bDispBW;

    QString m_qstrStorePath;
    int m_iMainTargetSeq;
    CStarMap* m_pstarmap;
    vector<sPacketGAE> m_vectpacketGAE;
    DllPhotometry* m_pPhotometry;
    double m_dblJB_XCOEF[6];
    double m_dblJB_YCOEF[6];
    ErrorResult m_errresPonit;
    QString m_qstrErrorResultPath;
    SNetMasterControlData m_SNetMasterControlData;
};

#endif // IMAGEPROCALGO_H
