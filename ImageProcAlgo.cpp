#include "ImageProcAlgo.h"

ImageProcAlgo::ImageProcAlgo()
{
    m_pGParam = GlobalParameter::GetInstance();

	m_bInit = false;
	m_szGrabWidth = 0;
	m_szGrabHeight = 0;
    m_szImageWidth  = 0;
    m_szImageHeight = 0;
	m_szCropWidth = 0;
	m_szCropHeight = 0;
	m_iBinning = 0;
	m_iProcMode = PROC_NONE;
	m_iTrackMode = TRACK_INIT;
	m_ulFrameSeq = 0;
	m_bDispEnhance = false;
    m_bDispBW = false;
    m_fAvr = 0;
    m_fStd = 0;
    m_bCLKernelInited = false;
    m_iMaxArea = 1000;
    m_iMinArea = 5;
    m_bAutoScale = true;
    m_uiScaleMax = 65535;
    m_uiScaleMin = 0;
    m_bProcImageSave = false;
    m_bCenterMode = true;
    m_bRotate = false;
    m_bNoSatuZone = true;
    m_bTrackProc = false;
    m_iNumBlob = 0;
    m_fRatioStd = 8.0;
    m_bDark = false;
    m_iNumMost = 0;
    m_qstrStorePath = "";
    m_iMainTargetSeq = -1;

    /// CL Kernel
	m_ckHist16 = NULL;
    m_ckRemoveBadLine = NULL;
    m_ckFlatBiasCorrect = NULL;
    m_ckLCM = NULL;
    m_ckReduceLargeDN16 = NULL;
    m_ckReduceSmallDN16 = NULL;
    m_ckPatch16 = NULL;
    m_ckDePatch8 = NULL;
    m_ckDePatch16 = NULL;
	m_ckShort2Byte = NULL;
	m_ckAvgGroupSum16 = NULL;
	m_ckVarGroupSum16 = NULL;
	m_ckAvgGroupSum8 = NULL;
	m_ckVarGroupSum8 = NULL;
	m_ckDoubleGlobalSum = NULL;
	m_ckMinMaxGroup16 = NULL;
	m_ckMinMaxGroup8 = NULL;
	m_ckDoubleGlobalMinMax = NULL;
	m_ckGaussianFilter16 = NULL;
	m_ckGaussianFilter8 = NULL;
	m_ckReduceSaturation16 = NULL;
	m_ckReduceSaturation8 = NULL;
	m_ckSubFrame16 = NULL;
	m_ckSubFrame8 = NULL;
	m_ckCrop16 = NULL;
	m_ckBinary16 = NULL;
	m_ckGrayDilate16 = NULL;
    m_ckGrayDilate8 = NULL;
	m_ckMedianFilter16 = NULL;
	m_ckSaltFilter8 = NULL;
	m_ckBinning16 = NULL;
	m_ckFrame5Median16 = NULL;
    m_ck_Rotate16 = NULL;
    m_ckGrayErode16 = NULL;
    m_ckBWErode8 = NULL;
    /// CL Memory
	m_cmGrab = NULL;
	m_cmOri = NULL;
    m_cmRemoveBadLine = NULL;
    m_cmRotate = NULL;
    m_cmPhotometry = NULL;
    m_cmMedian = NULL;
	m_cmEnhance = NULL;    
    m_cmCorrect = NULL;
    m_cmPitchEnhance = NULL;
    m_cmPitchEnhance1 = NULL;
    m_cmPitchEnhance2 = NULL;
    m_cmPitchEnhance3 = NULL;
    m_cmPitchEnhance4 = NULL;
    m_cmPitchEnhance5 = NULL;
    m_cmPitchNoStarBW = NULL;
    m_cmEnhanceMedian = NULL;
	m_cmEnhancePre = NULL;
	m_cmNoStar = NULL;
	m_cmNoStarBW = NULL;
	m_cmNoStarBW2 = NULL;
	m_cmCrop = NULL;
	m_cmCropBW = NULL;
	m_cmDisp = NULL;
	m_cmBg1 = NULL;
	m_cmBg2 = NULL;
	m_cmDilate = NULL;
	m_cmSubDilate = NULL;
	m_cmEnhance1 = NULL;
	m_cmEnhance2 = NULL;
	m_cmEnhance3 = NULL;
	m_cmEnhance4 = NULL;
	m_cmEnhance5 = NULL;
    m_cmFlat = NULL;
    m_cmFlatCrop = NULL;
    m_cmBias = NULL;
    m_cmBiasCrop = NULL;

    /// CPU Memory
    m_paucCropBW = NULL;
    m_paucNoStarBW = NULL;
    m_paucDisp = NULL;
    m_pausFlatFull = NULL;
    m_pausFlatCrop = NULL;
    m_pausBiasFull = NULL;
    m_pausBiasCrop = NULL;

    /// Init Photometry
    m_pPhotometry = nullptr;
//    InitPhotometry();

    /// Init GPU
    m_pGpuDevMngr = new DeviceManager;
    m_pGpuDevMngr->InitGPU("/home/dps/IniFiles/KernelCL.cl");
    InitCLKernel();

    /// Init Tracker
    m_pTrakcer = new TrackAlgo;

    /// Init Labler
    m_pLabeler = new Labeler;
    unsigned char* paucAcc = new unsigned char[64 * 64];
	memset(paucAcc, 0, 64 * 64 * sizeof(unsigned char));
	m_pLabeler->LabelEff(paucAcc, 64, 64, m_blobs, 0, 0, 64, 64);
	delete[] paucAcc;

    /// Load Bad Pixel
    LoadBadPixel("/home/dps/IniFiles/BadPixel.ini");

    /// Read Flat & Bias
    ReadFlatImage("/home/dps/IniFiles/Flat.raw");
    ReadBiasImage("/home/dps/IniFiles/Bias.raw");

    /// Read Point Error
    m_qstrErrorResultPath = "/home/dps/IniFiles/dSystemData.dat";
    ReadPointError(m_qstrErrorResultPath);

    /// Read JBCoef
    m_dblJB_XCOEF[0] = -0.055700352357762;
    m_dblJB_XCOEF[1] = -0.005662632539437;
    m_dblJB_XCOEF[2] = 0.000031896132376;
    m_dblJB_XCOEF[3] = 0.000000120999823;
    m_dblJB_XCOEF[4] = 0.000000057413609;
    m_dblJB_XCOEF[5] = 0.000000000788208;
    m_dblJB_YCOEF[0] = -0.034707582401088;
    m_dblJB_YCOEF[1] = 0.000042969168228;
    m_dblJB_YCOEF[2] = -0.005794715121020;
    m_dblJB_YCOEF[3] = 0.000000052425573;
    m_dblJB_YCOEF[4] = 0.000000117383973;
    m_dblJB_YCOEF[5] = 0.000000000791093;
    ReadJBCoef("/home/dps/IniFiles/JBCoef.ini", m_dblJB_XCOEF, m_dblJB_YCOEF);

    /// Init TWDW
    m_pstarmap = nullptr;
    LoadStarLib_TWDW();
    Init_TWDW();

    /// Set Photometry
//    SetPhotometry();
}
#include <QDebug>
ImageProcAlgo::~ImageProcAlgo()
{
	if (m_ckHist16)	clReleaseKernel(m_ckHist16);
    if (m_ckRemoveBadLine)	clReleaseKernel(m_ckRemoveBadLine);
    if (m_ckFlatBiasCorrect)	clReleaseKernel(m_ckFlatBiasCorrect);
    if (m_ckLCM)	clReleaseKernel(m_ckLCM);
    if (m_ckReduceLargeDN16)	clReleaseKernel(m_ckReduceLargeDN16);
    if (m_ckReduceSmallDN16)	clReleaseKernel(m_ckReduceSmallDN16);
    if (m_ckPatch16)	clReleaseKernel(m_ckPatch16);
    if (m_ckDePatch8)	clReleaseKernel(m_ckDePatch8);
    if (m_ckDePatch16)	clReleaseKernel(m_ckDePatch16);
	if (m_ckShort2Byte)	clReleaseKernel(m_ckShort2Byte);
	if (m_ckAvgGroupSum16)	clReleaseKernel(m_ckAvgGroupSum16);
	if (m_ckVarGroupSum16)	clReleaseKernel(m_ckVarGroupSum16);
	if (m_ckAvgGroupSum8)	clReleaseKernel(m_ckAvgGroupSum8);
	if (m_ckVarGroupSum8)	clReleaseKernel(m_ckVarGroupSum8);
	if (m_ckDoubleGlobalSum)	clReleaseKernel(m_ckDoubleGlobalSum);
	if (m_ckMinMaxGroup16)	clReleaseKernel(m_ckMinMaxGroup16);
	if (m_ckMinMaxGroup8)	clReleaseKernel(m_ckMinMaxGroup8);
	if (m_ckDoubleGlobalMinMax)	clReleaseKernel(m_ckDoubleGlobalMinMax);
	if (m_ckGaussianFilter16)	clReleaseKernel(m_ckGaussianFilter16);
	if (m_ckGaussianFilter8)	clReleaseKernel(m_ckGaussianFilter8);
	if (m_ckReduceSaturation16)	clReleaseKernel(m_ckReduceSaturation16);
	if (m_ckReduceSaturation8)	clReleaseKernel(m_ckReduceSaturation8);
	if (m_ckSubFrame16)	clReleaseKernel(m_ckSubFrame16);
	if (m_ckSubFrame8)	clReleaseKernel(m_ckSubFrame8);
	if (m_ckCrop16)	clReleaseKernel(m_ckCrop16);
	if (m_ckBinary16)	clReleaseKernel(m_ckBinary16);
	if (m_ckGrayDilate16)	clReleaseKernel(m_ckGrayDilate16);	
    if (m_ckGrayDilate8)	clReleaseKernel(m_ckGrayDilate8);
	if (m_ckGrayErode16)	clReleaseKernel(m_ckGrayErode16);	
    if (m_ckBWErode8)	clReleaseKernel(m_ckBWErode8);
	if (m_ckMedianFilter16)	clReleaseKernel(m_ckMedianFilter16);	
	if (m_ckSaltFilter8)	clReleaseKernel(m_ckSaltFilter8);	
	if (m_ckBinning16)	clReleaseKernel(m_ckBinning16);	
	if (m_ckFrame5Median16)	clReleaseKernel(m_ckFrame5Median16);	
    if (m_ck_Rotate16)	clReleaseKernel(m_ck_Rotate16);
	if (m_cmGrab)	clReleaseMemObject(m_cmGrab);
	if (m_cmOri)	clReleaseMemObject(m_cmOri);
    if (m_cmRemoveBadLine)	clReleaseMemObject(m_cmRemoveBadLine);
    if (m_cmRotate)	clReleaseMemObject(m_cmRotate);
    if (m_cmPhotometry)	clReleaseMemObject(m_cmPhotometry);
    if (m_cmMedian)	clReleaseMemObject(m_cmMedian);
	if (m_cmEnhance)	clReleaseMemObject(m_cmEnhance);
    if (m_cmCorrect)	clReleaseMemObject(m_cmCorrect);
    if (m_cmPitchEnhance)	clReleaseMemObject(m_cmPitchEnhance);
    if (m_cmPitchEnhance1)	clReleaseMemObject(m_cmPitchEnhance1);
    if (m_cmPitchEnhance2)	clReleaseMemObject(m_cmPitchEnhance2);
    if (m_cmPitchEnhance3)	clReleaseMemObject(m_cmPitchEnhance3);
    if (m_cmPitchEnhance4)	clReleaseMemObject(m_cmPitchEnhance4);
    if (m_cmPitchEnhance5)	clReleaseMemObject(m_cmPitchEnhance5);
    if (m_cmPitchNoStarBW)	clReleaseMemObject(m_cmPitchNoStarBW);
    if (m_cmEnhanceMedian)	clReleaseMemObject(m_cmEnhanceMedian);
	if (m_cmEnhancePre)	clReleaseMemObject(m_cmEnhancePre);	
	if (m_cmNoStar)	clReleaseMemObject(m_cmNoStar);
	if (m_cmNoStarBW)	clReleaseMemObject(m_cmNoStarBW);	
	if (m_cmNoStarBW2)	clReleaseMemObject(m_cmNoStarBW2);	
	if (m_cmCrop)	clReleaseMemObject(m_cmCrop);	
	if (m_cmCropBW)	clReleaseMemObject(m_cmCropBW);	
	if (m_cmDisp)	clReleaseMemObject(m_cmDisp);	
	if (m_cmBg1)	clReleaseMemObject(m_cmBg1);
	if (m_cmBg2)	clReleaseMemObject(m_cmBg2);
	if (m_cmDilate)	clReleaseMemObject(m_cmDilate);	
	if (m_cmSubDilate)	clReleaseMemObject(m_cmSubDilate);	
	if (m_cmEnhance1)	clReleaseMemObject(m_cmEnhance1);
	if (m_cmEnhance2)	clReleaseMemObject(m_cmEnhance2);
	if (m_cmEnhance3)	clReleaseMemObject(m_cmEnhance3);
	if (m_cmEnhance4)	clReleaseMemObject(m_cmEnhance4);
	if (m_cmEnhance5)	clReleaseMemObject(m_cmEnhance5);
    if (m_cmFlat)	clReleaseMemObject(m_cmFlat);
    if (m_cmFlatCrop)	clReleaseMemObject(m_cmFlatCrop);
    if (m_cmBias)	clReleaseMemObject(m_cmBias);
    if (m_cmBiasCrop)	clReleaseMemObject(m_cmBiasCrop);
	if (m_paucCropBW)	delete[] m_paucCropBW;
	if (m_paucNoStarBW)	delete[] m_paucNoStarBW;	
	if (m_paucDisp)	delete[] m_paucDisp;
    if (m_pausFlatFull) delete [] m_pausFlatFull;
    if (m_pausFlatCrop) delete [] m_pausFlatCrop;
    if (m_pausBiasFull) delete [] m_pausBiasFull;
    if (m_pausBiasCrop) delete [] m_pausBiasCrop;
	
	delete m_pGpuDevMngr;
	for (vector<sFramePacket>::iterator iter = m_vectFramePacket.begin(); iter != m_vectFramePacket.end(); iter++)
	{
		if ((unsigned short*)((*iter).sausImageGrab.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageGrab.pvoidBuffer);
		if ((unsigned short*)((*iter).sausImageEnhance.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
        if ((float*)((*iter).safImagePhotometry.pvoidBuffer))	delete[](float*)((*iter).safImagePhotometry.pvoidBuffer);
	}
	delete m_pTrakcer;
	if (!m_blobs.empty())
	{
		m_pLabeler->ReleaseBlobs(m_blobs);
	}
	delete m_pLabeler;

    /// 内存泄漏，但delete会在析构时崩溃
//    if(m_pstarmap)
//        delete m_pstarmap;
//    if(m_pPhotometry)
//        delete m_pPhotometry;

    qDebug() << "ImageProcAlgo";
}

int ImageProcAlgo::InitGPU(size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate)
{
    cl_int ciErrNum;
    m_bRotate = bRotate;
	if (((m_szGrabWidth != szGrabWidth) || (m_szGrabHeight != szGrabHeight) || (m_iBinning != iBinning)) 
		&& ((iBinning == 1) || (iBinning == 2) || (iBinning == 4)))
    {
		m_szGrabWidth = szGrabWidth;
		m_szGrabHeight = szGrabHeight;
		m_iBinning = iBinning;
		m_szImageWidth = m_szGrabWidth / m_iBinning;
		m_szImageHeight = m_szGrabHeight / m_iBinning;
		m_szCropWidth = szCropWidth;
		m_szCropHeight = szCropHeight;
        /// 释放显存
		if (m_cmGrab)	clReleaseMemObject(m_cmGrab);
		if (m_cmOri)	clReleaseMemObject(m_cmOri);
        if (m_cmRemoveBadLine)	clReleaseMemObject(m_cmRemoveBadLine);
        if (m_cmRotate)	clReleaseMemObject(m_cmRotate);
        if (m_cmPhotometry)	clReleaseMemObject(m_cmPhotometry);
        if (m_cmMedian)	clReleaseMemObject(m_cmMedian);
		if (m_cmEnhance)	clReleaseMemObject(m_cmEnhance);
        if (m_cmCorrect)	clReleaseMemObject(m_cmCorrect);        
        if (m_cmPitchEnhance)	clReleaseMemObject(m_cmPitchEnhance);
        if (m_cmPitchEnhance1)	clReleaseMemObject(m_cmPitchEnhance1);
        if (m_cmPitchEnhance2)	clReleaseMemObject(m_cmPitchEnhance2);
        if (m_cmPitchEnhance3)	clReleaseMemObject(m_cmPitchEnhance3);
        if (m_cmPitchEnhance4)	clReleaseMemObject(m_cmPitchEnhance4);
        if (m_cmPitchEnhance5)	clReleaseMemObject(m_cmPitchEnhance5);
        if (m_cmPitchNoStarBW)	clReleaseMemObject(m_cmPitchNoStarBW);
        if (m_cmEnhanceMedian)	clReleaseMemObject(m_cmEnhanceMedian);
		if (m_cmEnhancePre)	clReleaseMemObject(m_cmEnhancePre);
		if (m_cmNoStar)	clReleaseMemObject(m_cmNoStar);		
		if (m_cmNoStarBW)	clReleaseMemObject(m_cmNoStarBW);		
		if (m_cmNoStarBW2)	clReleaseMemObject(m_cmNoStarBW2);
		if (m_cmCrop)	clReleaseMemObject(m_cmCrop);	
		if (m_cmCropBW)	clReleaseMemObject(m_cmCropBW);		
		if (m_cmDisp)	clReleaseMemObject(m_cmDisp);	
		if (m_cmBg1)	clReleaseMemObject(m_cmBg1);
		if (m_cmBg2)	clReleaseMemObject(m_cmBg2);
		if (m_cmDilate)	clReleaseMemObject(m_cmDilate);
		if (m_cmSubDilate)	clReleaseMemObject(m_cmSubDilate);
		if (m_cmEnhance1)	clReleaseMemObject(m_cmEnhance1);
		if (m_cmEnhance2)	clReleaseMemObject(m_cmEnhance2);
		if (m_cmEnhance3)	clReleaseMemObject(m_cmEnhance3);
		if (m_cmEnhance4)	clReleaseMemObject(m_cmEnhance4);
		if (m_cmEnhance5)	clReleaseMemObject(m_cmEnhance5);
        if (m_paucCropBW)	delete [] m_paucCropBW;
		if (m_paucNoStarBW)	delete[] m_paucNoStarBW;		
        if (m_paucDisp)	delete[] m_paucDisp;
		/// 创建显存
		m_cmGrab = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szGrabWidth * m_szGrabHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmOri = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmRemoveBadLine = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmRotate = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPhotometry = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(float), NULL, &ciErrNum);
        m_cmMedian = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmCorrect = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance1 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance2 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance3 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance4 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchEnhance5 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmPitchNoStarBW = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 1024 * 1024 * sizeof(unsigned char), NULL, &ciErrNum);
        m_cmEnhanceMedian = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
        m_cmEnhancePre = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmNoStar = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmNoStarBW = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), NULL, &ciErrNum);
		m_cmNoStarBW2 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), NULL, &ciErrNum);
		m_cmCrop = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szCropWidth * m_szCropHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmCropBW = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szCropWidth * m_szCropHeight * sizeof(unsigned char), NULL, &ciErrNum);
		m_cmDisp = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), NULL, &ciErrNum);
		m_cmBg1 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmBg2 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmDilate = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmSubDilate = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance1 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance2 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance3 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance4 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_cmEnhance5 = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), NULL, &ciErrNum);
		m_paucCropBW = new unsigned char[m_szCropWidth * m_szCropHeight];
		m_paucNoStarBW = new unsigned char[m_szImageWidth * m_szImageHeight];	
		m_paucDisp = new unsigned char[m_szImageWidth * m_szImageHeight];
		m_bInit = true;
	}
	else
	{
		return 1;
	}

    return 0;
}

int ImageProcAlgo::InitCLKernel()
{
    cl_int ciErrNum;
    if (!m_bCLKernelInited)
    {
        /// 创建内核
        m_ckHist16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Hist16", &ciErrNum);
        m_ckRemoveBadLine = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_RemoveBadLine", &ciErrNum);
        m_ckFlatBiasCorrect = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_FlatBiasCorrect", &ciErrNum);
        m_ckLCM = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_LCM", &ciErrNum);
        m_ckReduceLargeDN16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_ReduceLargeDN16", &ciErrNum);
        m_ckReduceSmallDN16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_ReduceSmallDN16", &ciErrNum);
        m_ckPatch16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Patch16", &ciErrNum);
        m_ckDePatch8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_DePatch8", &ciErrNum);
        m_ckDePatch16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_DePatch16", &ciErrNum);
        m_ckShort2Byte = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Short2Byte", &ciErrNum);
        m_ckAvgGroupSum16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_AvgGroupSum16", &ciErrNum);
        m_ckVarGroupSum16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_VarGroupSum16", &ciErrNum);
        m_ckAvgGroupSum8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_AvgGroupSum8", &ciErrNum);
        m_ckVarGroupSum8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_VarGroupSum8", &ciErrNum);
        m_ckDoubleGlobalSum = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_DoubleGlobalSum", &ciErrNum);
        m_ckMinMaxGroup16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_MinMaxGroup16", &ciErrNum);
        m_ckMinMaxGroup8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_MinMaxGroup8", &ciErrNum);
        m_ckDoubleGlobalMinMax = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_DoubleGlobalMinMax", &ciErrNum);
        m_ckGaussianFilter16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_GaussianFilter16", &ciErrNum);
        m_ckGaussianFilter8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_GaussianFilter8", &ciErrNum);
        m_ckReduceSaturation16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_ReduceSaturation16", &ciErrNum);
        m_ckReduceSaturation8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_ReduceSaturation8", &ciErrNum);
        m_ckSubFrame16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_SubFrame16", &ciErrNum);
        m_ckSubFrame8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_SubFrame8", &ciErrNum);
        m_ckCrop16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Crop16", &ciErrNum);
        m_ckBinary16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Binary16", &ciErrNum);
        m_ckGrayDilate16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_GrayDilate16", &ciErrNum);
        m_ckGrayDilate8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_GrayDilate8", &ciErrNum);
        m_ckGrayErode16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_GrayErode16", &ciErrNum);
        m_ckBWErode8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_BWErode8", &ciErrNum);
        m_ckMedianFilter16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_MedianFilter16", &ciErrNum);
        m_ckSaltFilter8 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_SaltFilter8", &ciErrNum);
        m_ckBinning16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Binning16", &ciErrNum);
        m_ckFrame5Median16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Frame5Median16", &ciErrNum);
        m_ck_Rotate16 = clCreateKernel(m_pGpuDevMngr->cpProgram, "kernel_Rotate16", &ciErrNum);
        m_bCLKernelInited = true;
    }
}

int ImageProcAlgo::InitProc(int iProcMode, int iTrackMode, size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate, sTrackingSettings trackSettings)
{
    if (iProcMode == PROC_NONE
        || (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_MANUAL))
	{
        m_iProcMode = iProcMode;
		m_ulFrameSeq = 0;
        m_iNumBlob = 0;

        InitGPU(szGrabWidth, szGrabHeight, iBinning, szCropWidth, szCropHeight, bRotate);

        m_szCropWidth = szCropWidth;
        m_szCropHeight = szCropHeight;
        m_iTrackMode = iTrackMode;
        m_trackSettings = trackSettings;
        m_pTrakcer->InitTracker(iTrackMode, trackSettings, m_pGParam->m_SNetMasterControlData.qstrTargetID.toUInt());
        InitData();

        {
            for (vector<sFramePacket>::iterator iter = m_vectFramePacket.begin(); iter != m_vectFramePacket.end(); iter++)
            {
                if ((unsigned short*)((*iter).sausImageGrab.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageGrab.pvoidBuffer);
                if ((unsigned short*)((*iter).sausImageEnhance.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
                if ((float*)((*iter).safImagePhotometry.pvoidBuffer))	delete[](float*)((*iter).safImagePhotometry.pvoidBuffer);
            }
        }
        m_vectFramePacket.clear();

        qDebug() << "#################################";
        qDebug() << "### Init Proc";
        qDebug() << "#################################";
    }

	return 0;
}

int ImageProcAlgo::InputFramePacket(sFramePacket sPacket)
{
	if (m_bInit)
    {
        qDebug() << "### Image Seq:" << m_ulFrameSeq;
        ClockOn();
		sFramePacket sInputPacket;
		sInputPacket = sPacket;
		sInputPacket.sausImageEnhance.pvoidBuffer = NULL;
        sInputPacket.safImagePhotometry.pvoidBuffer = NULL;
		if (!sInputPacket.vectStarMeasures.empty()) sInputPacket.vectStarMeasures.clear();
		if (!sInputPacket.vectTargetMeasures.empty()) sInputPacket.vectTargetMeasures.clear();
		sInputPacket.sausImageGrab.pvoidBuffer = (void*)(new unsigned short[m_szGrabWidth * m_szGrabHeight]);
		memcpy((unsigned short*)sInputPacket.sausImageGrab.pvoidBuffer, (unsigned short*)sPacket.sausImageGrab.pvoidBuffer, m_szGrabWidth * m_szGrabHeight * sizeof(unsigned short));
        m_fExpTime = sPacket.fExpTime;
        {
            m_vectFramePacket.push_back(sInputPacket);
            qDebug() << "### Copy Image Buffer:" << ClockOff() << "ms";
            Proc_Rotate();
        }
	}
	else
	{
		return 1;
	}
	
    return 0;
}

int ImageProcAlgo::ProcFramePacket()
{
    if (m_bTrackProc)
    {
        switch (m_iProcMode)
        {
        case PROC_NONE:
            Proc_None();
            break;
        case PROC_DIFF:
            Proc_Diff();
            break;
        case PROC_DIRECT:
//            Proc_Direct();
            Proc_DirectPy();
            break;
        default:
            Proc_None();
            break;
        }
    }
    else
    {
        Proc_None();
    }
    m_ulFrameSeq++;
}

int ImageProcAlgo::Proc_Rotate(void)
{
    m_pGParam->m_SImageProcessorData.iProcTime = 0;
    ClockOn();
    if (m_vectFramePacket.size() > 8)
    {
        vector<sFramePacket>::iterator iter = m_vectFramePacket.begin();
        if ((unsigned short*)((*iter).sausImageGrab.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageGrab.pvoidBuffer);
        if ((unsigned short*)((*iter).sausImageEnhance.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
        if ((float*)((*iter).safImagePhotometry.pvoidBuffer))	delete[](float*)((*iter).safImagePhotometry.pvoidBuffer);
        m_vectFramePacket.erase(iter);
    }
    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;
    if ((*iter).ulFrameSeq == 0 || (*iter).ulFrameSeq == 1 || (*iter).ulFrameSeq == 2)
    {
        m_SNetMasterControlData = m_pGParam->m_SNetMasterControlData;
    }
    if ((*iter).sausImageGrab.pvoidBuffer != NULL
        && (*iter).sausImageEnhance.pvoidBuffer == NULL
        && (*iter).safImagePhotometry.pvoidBuffer == NULL)
    {
        /// 原始图像统计数据
        unsigned short* pausGrab = (unsigned short*)((*iter).sausImageGrab.pvoidBuffer);

        /// Read Image from Buffer
        if (m_iBinning == 1)
        {
            clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmOri, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausGrab, 0, NULL, NULL);
        }
        else
        {
            clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmGrab, CL_TRUE, 0, m_szGrabWidth * m_szGrabHeight * sizeof(unsigned short), pausGrab, 0, NULL, NULL);
            Binning16(m_cmGrab, m_cmOri, m_szGrabWidth, m_szGrabHeight, m_iBinning);
        }

        /// Rotate
        Rotate16(m_cmOri, m_cmRotate, m_cmPhotometry, m_szImageWidth, m_szImageHeight, m_bRotate, (*iter).sausImageGrab.dAvg, (*iter).sausImageGrab.dStd, m_bNoSatuZone);

        /// Remove Bad Line
        RemoveBadLine(m_cmRotate, m_cmRemoveBadLine, m_szImageWidth, m_szImageHeight);

        /// Write Image for Storage
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmRemoveBadLine, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), m_pGParam->m_SGrabberData.pausGrabDataRotate, 0, NULL, NULL);
    }
    m_pGParam->m_SImageProcessorData.iProcTime = ClockOff();
    qDebug() << "### PreProc & Write Image for Storage:" << m_pGParam->m_SImageProcessorData.iProcTime << "ms";
}

#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
int ImageProcAlgo::Proc_None(void)
{
	vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;

    if (m_cmRotate != NULL)
	{
        /// Avg,Std
        ClockOn();
        double dAvg, dStd;
        AvgStdGPU16(m_cmRemoveBadLine, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
        m_fAvr = dAvg;
        m_fStd = dStd;
        m_pGParam->m_SImageProcessorData.fAvr = m_fAvr;
        m_pGParam->m_SImageProcessorData.fStd = m_fStd;
        qDebug() << "### Proc AvgStd:" << ClockOff() << "ms";

        /// Median Filter
        ClockOn();
        MedianFilter16(m_cmRemoveBadLine, m_cmMedian, m_szImageWidth, m_szImageHeight, 3);  // m_cmMedian only used to display
        qDebug() << "### Proc Median:" << ClockOff() << "ms";

        /// 去背景增强 + Generate Image Enhance
        ClockOn();
        float fRatioStd = (m_pausFlatFull != NULL && m_pausBiasFull!= NULL) ? 0 : 1.0;
        SubBG16(m_cmRemoveBadLine, m_cmEnhance, 1.0, dAvg, dStd, fRatioStd, m_szImageWidth, m_szImageHeight);
        qDebug() << "### Proc Enhance:" << ClockOff() << "ms";

		/// 生成显示图像
        ClockOn();
        int iMax, iMin;
        if (!m_bDispEnhance)	// 显示图像不增强
        {
            int iMax, iMin;
            if (m_bAutoScale)
			{
                Short2ByteAutoGPU(m_cmEnhance, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
			}
			else
			{
                Short2ByteMinMaxGPU(m_cmEnhance, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
			}
		}
        else // 显示图像增强
		{
			if (m_bAutoScale)
			{
//                Short2ByteAutoGPU(m_cmMedian, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
                unsigned int uiScaleMin = m_fAvr - 3 * m_fStd < 0 ? 0 : (unsigned int)(m_fAvr - 3 * m_fStd);
                unsigned int uiScaleMax = m_fAvr + 3 * m_fStd > 16383 ? 16383 : (unsigned int)(m_fAvr + 3 * m_fStd);
                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, uiScaleMin, uiScaleMax, m_szImageWidth, m_szImageHeight);
			}
			else
			{
                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
			}
		}	
        qDebug() << "### Proc Disp:" << ClockOff() << "ms";

        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmDisp, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucDisp, 0, NULL, NULL);
    }

	return 0;
}

int ImageProcAlgo::Proc_Diff(void)
{
    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;
    bool bCropBlob;

    if (m_cmRotate != NULL)
    {
        /// Generate Image Photometry
        ClockOn();
        (*iter).safImagePhotometry.pvoidBuffer = (void*)new float[m_szImageWidth * m_szImageHeight];
        float* pafImagePhotometry = (float*)((*iter).safImagePhotometry.pvoidBuffer);
        memset(pafImagePhotometry, 0, m_szImageWidth * m_szImageHeight * sizeof(float));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmPhotometry, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(float), pafImagePhotometry, 0, NULL, NULL);
        qDebug() << "### Write Image for Photometry:" << ClockOff() << "ms";

        /// Avg,Std
        ClockOn();
        double dAvg, dStd;
        AvgStdGPU16(m_cmRemoveBadLine, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
        m_fAvr = dAvg;
        m_fStd = dStd;
        m_pGParam->m_SImageProcessorData.fAvr = m_fAvr;
        m_pGParam->m_SImageProcessorData.fStd = m_fStd;
        qDebug() << "### Proc AvgStd:" << ClockOff() << "ms";

        /// Median Filter
        ClockOn();
        MedianFilter16(m_cmRemoveBadLine, m_cmMedian, m_szImageWidth, m_szImageHeight, 2.5);
        qDebug() << "### Proc Median:" << ClockOff() << "ms";

        /// 去背景增强 + Generate Image Enhance
        ClockOn();
        (*iter).sausImageEnhance.pvoidBuffer = (void*)new unsigned short[m_szImageWidth * m_szImageHeight];
        unsigned short* pausImageEnhance = (unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
        memset(pausImageEnhance, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short));
        clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmEnhance, m_cmEnhancePre, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), 0, NULL, NULL);	// 增强图像向前帧显存拷贝
        float fRatioStd = (m_pausFlatFull != NULL && m_pausBiasFull!= NULL) ? 0 : 1.0;
        SubBG16(m_cmRemoveBadLine, m_cmEnhance, 1.0, dAvg, dStd, fRatioStd, m_szImageWidth, m_szImageHeight);
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmEnhance, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausImageEnhance, 0, NULL, NULL);
        qDebug() << "### Write Enhanced Image:" << ClockOff() << "ms";

        /// 增强图像裁剪与二值化
        ClockOn();
        Crop16(m_cmEnhance, m_cmCrop, m_szImageWidth, m_szImageHeight, m_szCropWidth, m_szCropHeight);
        AvgStdGPU16(m_cmCrop, &((*iter).sausImageEnhance.dAvg), &((*iter).sausImageEnhance.dStd), m_szCropWidth, m_szCropHeight);
        Binary16(m_cmCrop, m_cmCropBW, m_szCropWidth, m_szCropHeight, (unsigned short)((*iter).sausImageEnhance.dAvg + 2.5 * (*iter).sausImageEnhance.dStd));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmCropBW, CL_TRUE, 0, m_szCropWidth * m_szCropHeight * sizeof(unsigned char), m_paucCropBW, 0, NULL, NULL);

        /// 裁剪图像求连通域
        if (!m_blobs.empty())
        {
            m_pLabeler->ReleaseBlobs(m_blobs);
        }
        m_pLabeler->LabelEff(m_paucCropBW, m_szCropWidth, m_szCropHeight, m_blobs, 0, 0, m_szCropWidth, m_szCropHeight);
        m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
        if (m_blobs.size() > 0)
        {
            for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
            {
                sMeasureBlob blob;
                pair<float, float> pairfRealPos = pair<float, float>(0, 0);
                pair<float, float> pairfCropPos = pair<float, float>(0, 0);
                pairfCropPos = pair<float, float>((*iterB).second->centroid_x, (*iterB).second->centroid_y);
                CropPos2RealPos(pairfCropPos, pairfRealPos, m_szImageWidth, m_szImageHeight, m_szCropWidth, m_szCropHeight);
                blob.pairfPos.first = pairfRealPos.first * m_iBinning;
                blob.pairfPos.second = pairfRealPos.second * m_iBinning;
                pairfCropPos = pair<float, float>((*iterB).second->minx, (*iterB).second->miny);
                CropPos2RealPos(pairfCropPos, pairfRealPos, m_szImageWidth, m_szImageHeight, m_szCropWidth, m_szCropHeight);
                blob.fMinX = pairfRealPos.first * m_iBinning;
                blob.fMinY = pairfRealPos.second * m_iBinning;
                pairfCropPos = pair<float, float>((*iterB).second->maxx, (*iterB).second->maxy);
                CropPos2RealPos(pairfCropPos, pairfRealPos, m_szImageWidth, m_szImageHeight, m_szCropWidth, m_szCropHeight);
                blob.fMaxX = pairfRealPos.first * m_iBinning;
                blob.fMaxY = pairfRealPos.second * m_iBinning;
                blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
                blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
                (*iter).vectStarMeasures.push_back(blob);	// 向帧数据内添加恒星位置数据
            }
            /// 计算恒星运动速度
            sMeasuresInFrame measuresStar;
            measuresStar.fExpTime = (*iter).fExpTime;
            measuresStar.fFrameFreq = (*iter).fFrameFreq;
            measuresStar.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
            measuresStar.stimeFrame = (*iter).stimeFrame;
            measuresStar.ulFrameSeq = (*iter).ulFrameSeq;
            measuresStar.vectStarMeasures = (*iter).vectStarMeasures;
            pair<float, float> pairfSpd, pairfSpd_AE;
            int iNumMost = 0;
            m_pTrakcer->CalcStarSpd(measuresStar, pairfSpd, pairfSpd_AE, iNumMost);
            pairfSpd.first /= m_iBinning;
            pairfSpd.second /= m_iBinning;

            qDebug() << "### Proc CalcStarSpd:" << ClockOff() << "ms";
            qDebug() << "### Crop Blobs:" << measuresStar.vectStarMeasures.size();
            qDebug() << "### Star speed: (" << pairfSpd.first << ", " << pairfSpd.second << ").  NumMost: " << iNumMost << ".";

            /// 帧差去恒星与二值化
            if (m_ulFrameSeq > 0)
            {
                int iDilateSize = 3;
                int iErodeSize = 3;
                /// 第0帧图像处理
                if (m_ulFrameSeq == 1)
                {
                    /// 帧差去背景
                    ClockOn();
                    pair<float, float> pairfSpdReverse;
                    pairfSpdReverse.first = -pairfSpd.first;
                    pairfSpdReverse.second = -pairfSpd.second;
                    SubFrameNoStar(m_cmEnhancePre, m_cmEnhance, m_cmNoStar, m_szImageWidth, m_szImageHeight, iDilateSize, iErodeSize, pairfSpdReverse);
                    qDebug() << "### Proc SubFrame:" << ClockOff() << "ms";

                    /// 二值化
                    ClockOn();
                    AvgStdGPU16(m_cmNoStar, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
                    Binary16(m_cmNoStar, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg + 2.5 * dStd));
                    if (pairfSpd.first == 0 && pairfSpd.second == 0)	// 恒动模式
                    {
                        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
                    }
                    else // 凝视模式
                    {
                        SaltFilter8(m_cmNoStarBW, m_cmNoStarBW2, m_szImageWidth, m_szImageHeight);
                        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW2, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
                    }
                    qDebug() << "### Proc Binary:" << ClockOff() << "ms";

                    /// 连通域提取
                    ClockOn();
                    if (!m_blobs.empty())
                    {
                        m_pLabeler->ReleaseBlobs(m_blobs);
                    }
                    m_pLabeler->LabelEff(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
                    m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
                    RemoveBadPixel(m_blobs);
                    RemoveBadBlob(m_blobs);
                    /// 质心计算
                    vector<sMeasureBlob> vectTargetMeasures;
                    if (m_blobs.size() > 0)
                    {
                        m_pLabeler->Centroids((unsigned short*)((*(iter - 1)).sausImageEnhance.pvoidBuffer), m_szImageWidth, m_szImageHeight, m_blobs, 5, 1.0, 1.0);
                        for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
                        {
                            sMeasureBlob blob;
                            blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
                            blob.fMinX = (*iterB).second->minx * m_iBinning;
                            blob.fMinY = (*iterB).second->miny * m_iBinning;
                            blob.fMaxX = (*iterB).second->maxx * m_iBinning;
                            blob.fMaxY = (*iterB).second->maxy * m_iBinning;
                            blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
                            blob.pairfPosAE.second = (*(iter - 1)).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                            blob.pairfPosAE.first = (*(iter - 1)).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
                            vectTargetMeasures.push_back(blob);
                        }
                    }
                    qDebug() << "### Proc Blob:" << ClockOff() << "ms";
                    /// 测量数据打包
                    sMeasuresInFrame measuresTarget;
                    measuresTarget.fExpTime = (*(iter - 1)).fExpTime;
                    measuresTarget.fFrameFreq = (*(iter - 1)).fFrameFreq;
                    measuresTarget.pairfFOVCenterAE = (*(iter - 1)).pairfFOVCenterAE;
                    measuresTarget.stimeFrame = (*(iter - 1)).stimeFrame;
                    measuresTarget.ulFrameSeq = (*(iter - 1)).ulFrameSeq;
                    measuresTarget.vectTargetMeasures = vectTargetMeasures;
                    /// 目标检测处理
                    ClockOn();
                    m_pTrakcer->TrackProc_GEO(measuresTarget, m_vectTargetInfo, false, vector<pair<QString, sMeasureBlob>>());  //diff需要修改track_geo
                    qDebug() << "### Proc Track:" << ClockOff() << "ms";
                }
                /// 第1~N帧图像处理
                /// 帧差去背景
                ClockOn();
                SubFrameNoStar(m_cmEnhance, m_cmEnhancePre, m_cmNoStar, m_szImageWidth, m_szImageHeight, iDilateSize, iErodeSize, pairfSpd);
                qDebug() << "### Proc SubFrame:" << ClockOff() << "ms";

                /// 二值化
                ClockOn();
                AvgStdGPU16(m_cmNoStar, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
                Binary16(m_cmNoStar, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg + 2.5 * dStd));
                if (pairfSpd.first == 0 && pairfSpd.second == 0)	// 恒动模式
                {
                    clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
                }
                else // 凝视模式
                {
                    SaltFilter8(m_cmNoStarBW, m_cmNoStarBW2, m_szImageWidth, m_szImageHeight);
                    clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW2, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
                }
                qDebug() << "### Proc Binary:" << ClockOff() << "ms";

                /// 连通域提取
                ClockOn();
                if (!m_blobs.empty())
                {
                    m_pLabeler->ReleaseBlobs(m_blobs);
                }
                m_pLabeler->LabelEff(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
                m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
                RemoveBadPixel(m_blobs);
                RemoveBadBlob(m_blobs);
                qDebug() << "### Diff Blobs:" << m_blobs.size();
                /// 质心计算
                vector<sMeasureBlob> vectTargetMeasures;
                if (m_blobs.size() > 0)
                {
                    m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 5, 1.0, 1.0);
                    for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
                    {
                        sMeasureBlob blob;
                        blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
                        blob.fMinX = (*iterB).second->minx * m_iBinning;
                        blob.fMinY = (*iterB).second->miny * m_iBinning;
                        blob.fMaxX = (*iterB).second->maxx * m_iBinning;
                        blob.fMaxY = (*iterB).second->maxy * m_iBinning;
                        blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
                        blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                        blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
                        vectTargetMeasures.push_back(blob);
                    }
                }
                qDebug() << "### Proc Blob:" << ClockOff() << "ms";
                /// 测量数据打包
                sMeasuresInFrame measuresTarget;
                measuresTarget.fExpTime = (*iter).fExpTime;
                measuresTarget.fFrameFreq = (*iter).fFrameFreq;
                measuresTarget.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
                measuresTarget.stimeFrame = (*iter).stimeFrame;
                measuresTarget.ulFrameSeq = (*iter).ulFrameSeq;
                measuresTarget.vectTargetMeasures = vectTargetMeasures;
                /// 目标检测处理
                ClockOn();
                m_pTrakcer->TrackProc_GEO(measuresTarget, m_vectTargetInfo, false, vector<pair<QString, sMeasureBlob>>());  //diff需要修改track_geo
                qDebug() << "### Proc Track:" << ClockOff() << "ms";
            }
            bCropBlob = true;
        }
        else
        {
            bCropBlob = false;
        }
        /// 生成显示图像
        ClockOn();
        if (!m_bDispEnhance)	// 显示图像不增强
        {
//                if (m_bAutoScale)
//                {
//                    int iMax, iMin;
//                    Short2ByteAutoGPU(m_cmEnhance, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
//                }
//                else
//                {
//                    Short2ByteMinMaxGPU(m_cmEnhance, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
//                }
            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, m_cmDisp, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), 0, NULL, NULL);
        }
        else // 显示图像增强
        {
            if (m_bAutoScale)
            {
//                int iMax, iMin;
//                Short2ByteAutoGPU(m_cmMedian, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
                unsigned int uiScaleMin = m_fAvr - 3 * m_fStd < 0 ? 0 : (unsigned int)(m_fAvr - 3 * m_fStd);
                unsigned int uiScaleMax = m_fAvr + 3 * m_fStd > 16383 ? 16383 : (unsigned int)(m_fAvr + 3 * m_fStd);
                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, uiScaleMin, uiScaleMax, m_szImageWidth, m_szImageHeight);
            }
            else
            {
                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
            }
        }
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmDisp, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucDisp, 0, NULL, NULL);
        qDebug() << "### Proc Disp:" << ClockOff() << "ms";
    }

    return bCropBlob ? 0 : 1;
}

//int ImageProcAlgo::Proc_Direct(void)
//{
//    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;

//    if (m_cmRotate != NULL)
//    {
//        /// Generate Image Photometry
//        ClockOn();
//        (*iter).safImagePhotometry.pvoidBuffer = (void*)new float[m_szImageWidth * m_szImageHeight];
//        float* pafImagePhotometry = (float*)((*iter).safImagePhotometry.pvoidBuffer);
//        memset(pafImagePhotometry, 0, m_szImageWidth * m_szImageHeight * sizeof(float));
//        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmPhotometry, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(float), pafImagePhotometry, 0, NULL, NULL);
//        qDebug() << "### Write Image for Photometry:" << ClockOff() << "ms";

//        /// Avg,Std
//        ClockOn();
//        double dAvg, dStd;
//        AvgStdGPU16(m_cmRemoveBadLine, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
//        m_fAvr = dAvg;
//        m_fStd = dStd;
//        m_pGParam->m_SImageProcessorData.fAvr = m_fAvr;
//        m_pGParam->m_SImageProcessorData.fStd = m_fStd;
//        qDebug() << "### Proc AvgStd:" << ClockOff() << "ms";

//        /// Median Filter
//        ClockOn();
//        MedianFilter16(m_cmRemoveBadLine, m_cmMedian, m_szImageWidth, m_szImageHeight, 3);  // m_cmMedian only used to display
//        qDebug() << "### Proc Median:" << ClockOff() << "ms";

//        /// 去背景增强 + Generate Image Enhance
//        ClockOn();
//        float fRatioStd = (m_pausFlatFull != NULL && m_pausBiasFull!= NULL) ? 0 : 1.0;
//        SubBG16(m_cmRemoveBadLine, m_cmEnhance, 1.0, dAvg, dStd, fRatioStd, m_szImageWidth, m_szImageHeight);
//        qDebug() << "### Proc SubBG:" << ClockOff() << "ms";

//        /// Median Filter and Dilate Gray on Image Enhance
//        ClockOn();
//        if (m_iTrackMode == TRACK_GEO || m_iTrackMode == TRACK_MANUAL)
//        {
//            /// Median Filter
//            MedianFilter16(m_cmEnhance, m_cmEnhanceMedian, m_szImageWidth, m_szImageHeight, 3);

//            /// Dilate Gray
//            DilateGray16(m_cmEnhanceMedian, m_cmDilate, m_szImageWidth, m_szImageHeight, 5);
//        }
//        else
//        {
//            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmEnhance, m_cmDilate, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), 0, NULL, NULL);
//        }
//        qDebug() << "### Proc Median & Dilate:" << ClockOff() << "ms";

//        /// Generate Image Enhance
//        ClockOn();
//        (*iter).sausImageEnhance.pvoidBuffer = (void*)new unsigned short[m_szImageWidth * m_szImageHeight];
//        unsigned short* pausImageEnhance = (unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
//        memset(pausImageEnhance, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short));
//        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmDilate, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausImageEnhance, 0, NULL, NULL);
//        qDebug() << "### Generate Image Enhance:" << ClockOff() << "ms";

//        /// 确定m_fRatioStd
//        ClockOn();
//        if (m_iTrackMode == TRACK_GEO)
//        {

//            /// 增强图像裁剪与二值化
//            Crop16(m_cmDilate, m_cmCrop, m_szImageWidth, m_szImageHeight, m_szCropWidth, m_szCropHeight);
//            AvgStdGPU16(m_cmCrop, &((*iter).sausImageEnhance.dAvg), &((*iter).sausImageEnhance.dStd), m_szCropWidth, m_szCropHeight);
//            Binary16(m_cmCrop, m_cmCropBW, m_szCropWidth, m_szCropHeight, (unsigned short)((*iter).sausImageEnhance.dAvg + m_fRatioStd * (*iter).sausImageEnhance.dStd));
//            clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmCropBW, CL_TRUE, 0, m_szCropWidth * m_szCropHeight * sizeof(unsigned char), m_paucCropBW, 0, NULL, NULL);

//            /// 裁剪图像求连通域
//            if (!m_blobs.empty())
//            {
//                m_pLabeler->ReleaseBlobs(m_blobs);
//            }
//            m_pLabeler->LabelEff(m_paucCropBW, m_szCropWidth, m_szCropHeight, m_blobs, 0, 0, m_szCropWidth, m_szCropHeight);
//            m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);

//            if (m_ulFrameSeq == 0)
//            {
//                m_iNumBlob = m_blobs.size();
//                if (m_iNumBlob > 200)
//                {
//                    m_fRatioStd = 5.0;
//                }
//                else
//                {
//                    m_fRatioStd = 2.5;
//                }
//            }
//            else if (m_ulFrameSeq == 1)
//            {
//                m_iNumBlob = m_blobs.size();
//            }
//        }
//        else
//        {
//            m_fRatioStd = 4.0;
//        }
//        if (m_bDark) m_fRatioStd = 1.5;
//        if (m_fExpTime <= 100) m_fRatioStd = 3.0;
//        qDebug() << "### Number of Crop Blob:" << m_iNumBlob;
//        qDebug() << "### fRatioStd:" << m_fRatioStd;
//        qDebug() << "### Proc Crop Binary:" << ClockOff() << "ms";
//        m_pGParam->m_SImageProcessorData.uiNumBlob = m_blobs.size();
//        m_pGParam->m_SImageProcessorData.uiCropWidth = m_szCropWidth;
//        m_pGParam->m_SImageProcessorData.uiCropHeight = m_szCropHeight;
//        m_pGParam->m_SImageProcessorData.fRatioStd = m_fRatioStd;


//        /// 二值化
//        ClockOn();
//        if ((m_iTrackMode == TRACK_GEO && (*iter).fExpTime < 100.0)
//                || m_iTrackMode == TRACK_SC
//                || m_iTrackMode == TRACK_LEO)
//        {
//            Binary16(m_cmRemoveBadLine, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg + m_fRatioStd * dStd));
//        }
//        else
//        {
//            AvgStdGPU16(m_cmDilate, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
//            Binary16(m_cmDilate, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg + m_fRatioStd * dStd));
//        }
//        SaltFilter8(m_cmNoStarBW, m_cmNoStarBW2, m_szImageWidth, m_szImageHeight);
//        if (m_iTrackMode == TRACK_GEO || m_iTrackMode == TRACK_MANUAL)
//        {
//            DilateGray8(m_cmNoStarBW2, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, 3);
//        }
//        else
//        {
//            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW2, m_cmNoStarBW, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), 0, NULL, NULL);
//        }
//        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
//        qDebug() << "### Proc Binary:" << ClockOff() << "ms";

//        /// 连通域提取
//        ClockOn();
//        if (!m_blobs.empty())
//        {
//            m_pLabeler->ReleaseBlobs(m_blobs);
//        }
//        m_pLabeler->LabelEff(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
//        for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
//        {
//            sMeasureBlob blob;
//            blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
//            blob.fMinX = (*iterB).second->minx * m_iBinning;
//            blob.fMinY = (*iterB).second->miny * m_iBinning;
//            blob.fMaxX = (*iterB).second->maxx * m_iBinning;
//            blob.fMaxY = (*iterB).second->maxy * m_iBinning;
//            blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
//            blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
//            blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
//        }
//        m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
//        RemoveBadPixel(m_blobs);
//        RemoveBadBlob(m_blobs);
//        m_iNumBlob = m_blobs.size();
//        /// 质心计算
//        vector<sMeasureBlob> vectTargetMeasures;
//        if (m_blobs.size() > 0/* && m_blobs.size() < 1000*/)
//        {
//            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 5, 1.0, 1.0);
//            for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
//            {
//                sMeasureBlob blob;
//                blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
//                blob.fMinX = (*iterB).second->minx * m_iBinning;
//                blob.fMinY = (*iterB).second->miny * m_iBinning;
//                blob.fMaxX = (*iterB).second->maxx * m_iBinning;
//                blob.fMaxY = (*iterB).second->maxy * m_iBinning;
//                blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
//                blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
//                blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
//                vectTargetMeasures.push_back(blob);
//            }
//        }
//        qDebug() << "### Proc Blob:" << ClockOff() << "ms";

//        /// 向帧数据内添加恒星位置数据
//        (*iter).vectStarMeasures = vectTargetMeasures;
//        /// 测量数据打包
//        sMeasuresInFrame measuresTarget;
//        measuresTarget.fExpTime = (*iter).fExpTime;
//        measuresTarget.fFrameFreq = (*iter).fFrameFreq;
//        measuresTarget.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
//        measuresTarget.stimeFrame = (*iter).stimeFrame;
//        measuresTarget.ulFrameSeq = (*iter).ulFrameSeq;
//        measuresTarget.dTemp = (*iter).fTemp;
//        measuresTarget.dAtmosP = (*iter).fAtmosP;
//        measuresTarget.vectTargetMeasures = vectTargetMeasures;

//        /// 计算恒星运动速度
//        ClockOn();
//        if (m_iTrackMode == TRACK_GEO)
//        {
//            sMeasuresInFrame measuresStar;
//            measuresStar.fExpTime = (*iter).fExpTime;
//            measuresStar.fFrameFreq = (*iter).fFrameFreq;
//            measuresStar.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
//            measuresStar.stimeFrame = (*iter).stimeFrame;
//            measuresStar.ulFrameSeq = (*iter).ulFrameSeq;

//            int iNumStar = (*iter).vectStarMeasures.size();

//            float fRatioFOV = (500 / (float)iNumStar);
//            fRatioFOV = fRatioFOV >= 1 ? 1 : fRatioFOV;

//            size_t szCropWidth = (size_t)(m_szGrabWidth * pow(fRatioFOV, 0.5));
//            size_t szCropHeight = (size_t)(m_szGrabHeight * pow(fRatioFOV, 0.5));

//            /// 兼容短曝光全帧目标跟踪,恒星搜索范围变大
//            if ((*iter).fExpTime < 200.0)
//            {
//                szCropWidth = 2048;
//                szCropHeight = 2048;
//            }
//            if ((*iter).fExpTime < 100.0)
//            {
//                szCropWidth = 4096;
//                szCropHeight = 4096;
//            }

//            for (int i = 0; i < (*iter).vectStarMeasures.size(); i++)
//            {
//                if ((*iter).vectStarMeasures[i].pairfPos.first > m_pGParam->m_SOpticData.fOptCenterX - szCropWidth * 0.5
//                        && (*iter).vectStarMeasures[i].pairfPos.first < m_pGParam->m_SOpticData.fOptCenterX + szCropWidth * 0.5
//                        && (*iter).vectStarMeasures[i].pairfPos.second > m_pGParam->m_SOpticData.fOptCenterY - szCropHeight * 0.5
//                        && (*iter).vectStarMeasures[i].pairfPos.second < m_pGParam->m_SOpticData.fOptCenterY + szCropHeight * 0.5)
//                {
//                    measuresStar.vectStarMeasures.push_back((*iter).vectStarMeasures[i]);
//                }
//            }

//            pair<float, float> pairfSpd, pairfSpd_AE;
//            m_iNumMost = 0;
//            m_pTrakcer->CalcStarSpd(measuresStar, pairfSpd, pairfSpd_AE, m_iNumMost);

//            pairfSpd.first /= m_iBinning;
//            pairfSpd.second /= m_iBinning;
//            qDebug() << "### Star speed: (" << pairfSpd.first << "," << pairfSpd.second << ");  NumMost:" << m_iNumMost << ";  CropWidth:" << szCropWidth << ";  CropHeight: " << szCropHeight ;
//            qDebug() << "### Full blobs:" << m_blobs.size() << "; Crop blobs:" << measuresStar.vectStarMeasures.size();
//            m_pGParam->m_SImageProcessorData.pairfStarSpd = pairfSpd;
//            m_pGParam->m_SImageProcessorData.uiNumStarMatch = m_iNumMost;
//        }
//        else
//        {
//            qDebug() << "### Full blobs:" << m_blobs.size();
//        }
//        qDebug() << "### Proc CalcStarSpd:" << ClockOff() << "ms";

//        /// 目标检测跟踪处理
//        ClockOn();
//        if (m_iTrackMode == TRACK_GEO)
//        {
//            m_pGParam->m_SImageProcessorData.bFullLEO = (*iter).fExpTime < 100.0;
//            if (m_iNumMost >= 5 || (*iter).fExpTime < 100.0)    // calculate star speed success or LEO Tracking
//            {
//                m_pTrakcer->TrackProc_GEO(measuresTarget, m_vectTargetInfo, (*iter).fExpTime < 100.0);
//            }
//        }
//        else if (m_iTrackMode == TRACK_SC)
//        {
//            m_pTrakcer->TrackProc_SC(measuresTarget, m_targetInfo);
//        }
//        else if (m_iTrackMode == TRACK_LEO)
//        {
//            m_pTrakcer->TrackProc_LEO(measuresTarget, m_targetInfo);
//        }
//        else if (m_iTrackMode == TRACK_MANUAL)
//        {
//            if (m_pGParam->m_SImageProcessorData.bDoubleClicked)
//            {
//                m_pTrakcer->TrackProc_MANUAL(measuresTarget, m_targetInfo, m_pGParam->m_SImageProcessorData.pairfClickedPos);
//            }
//        }
//        m_pGParam->m_SImageProcessorData.uiProcTrackMs = ClockOff();
//        qDebug() << "### Proc Track:" << m_pGParam->m_SImageProcessorData.uiProcTrackMs << "ms";

//        if (m_iTrackMode == TRACK_GEO)
//        {
//            if (m_vectTargetInfo.size() > 25)
//            {
//                m_fRatioStd = 5.0;
//            }
//        }
//        m_pGParam->m_SImageProcessorData.uiNumMeasure = measuresTarget.vectTargetMeasures.size();

//        ClockOn();
//        if (!m_bDispEnhance)	// 显示图像不增强
//        {
////            if (m_bAutoScale)
////            {
////                int iMax, iMin;
////                Short2ByteAutoGPU(m_cmEnhanceMedian, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
////            }
////            else
////            {
////                Short2ByteMinMaxGPU(m_cmEnhanceMedian, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
////            }
//            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, m_cmDisp, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), 0, NULL, NULL);
//        }
//        else // 显示图像增强
//        {
//            if (m_bAutoScale)
//            {
////				int iMax, iMin;
////                Short2ByteAutoGPU(m_cmMedian, m_cmDisp, m_szImageWidth, m_szImageHeight, &iMin, &iMax, 1, 1, -1, m_szImageWidth * m_szImageHeight * 0.01);
//                unsigned int uiScaleMin = m_fAvr - 3 * m_fStd < 0 ? 0 : (unsigned int)(m_fAvr - 3 * m_fStd);
//                unsigned int uiScaleMax = m_fAvr + 3 * m_fStd > 16383 ? 16383 : (unsigned int)(m_fAvr + 3 * m_fStd);
//                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, uiScaleMin, uiScaleMax, m_szImageWidth, m_szImageHeight);
//            }
//            else
//            {
//                Short2ByteMinMaxGPU(m_cmMedian, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
//            }
//        }
//        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmDisp, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucDisp, 0, NULL, NULL);
//        qDebug() << "### Proc Disp:" << ClockOff() << "ms";
//    }

//    return 0;
//}


void ImageProcAlgo::WriteJsonFile(bool busePosAE)
{
    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;

    QJsonObject jsonObject; // 创建一个JSON对象并设置数据
    jsonObject.insert("imagew", static_cast<int>(m_szImageWidth));
    jsonObject.insert("imageh", static_cast<int>(m_szImageHeight));
    QDateTime curFrameTime(QDate(iter->stimeFrame.iYear, iter->stimeFrame.iMonth, iter->stimeFrame.iDay), QTime(iter->stimeFrame.iHour, iter->stimeFrame.iMinute, iter->stimeFrame.iSecond, iter->stimeFrame.iMillisecond));
    jsonObject.insert("date-obs", curFrameTime.toString("yyyy-MM-ddThh:mm:ss.zzz"));
    jsonObject.insert("bjt", true);
    jsonObject.insert("exposure", iter->fExpTime);
    jsonObject.insert("temperature", iter->fTemp);
    jsonObject.insert("pressure", iter->fAtmosP * 0.01);
    jsonObject.insert("humidity", iter->fHumidity);
    jsonObject.insert("lon", m_pGParam->m_SObsParams.fLongitude);
    jsonObject.insert("lat", m_pGParam->m_SObsParams.fLatitude);
    jsonObject.insert("height", m_pGParam->m_SObsParams.fAltitude);
    jsonObject.insert("use_point", busePosAE);
    jsonObject.insert("point_az", iter->pairfFOVCenterAEModify.first);
    jsonObject.insert("point_el", iter->pairfFOVCenterAEModify.second);
    jsonObject.insert("radius", 1.5);
    jsonObject.insert("pixscale", m_trackSettings.opticparams.fPixelScale);
    jsonObject.insert("tweak-order", 3);

    // 创建 JSON 文档
    QJsonDocument jsonDoc(jsonObject);

    // 将JSON文档写入文件
//    QString filePath = "/home/dps/PrevFile/Settings/" + m_pGParam->m_SImageReplayerData.qstrCurFileName + "Settings.json"; // 修改为你想保存的文件路径
    int lastDotIndex = m_pGParam->m_SImageReplayerData.qstrCurFileName.lastIndexOf('/');
    QString qstrJsonSaveDir = m_pGParam->m_SImageReplayerData.qstrCurFileName.left(lastDotIndex) + "/Settings", filename = m_pGParam->m_SImageReplayerData.qstrCurFileName.mid(lastDotIndex + 1) + "Settings.json";
    QDir qdirJson;
    if (!qdirJson.exists(qstrJsonSaveDir))
        if (qdirJson.mkpath(qstrJsonSaveDir))
            qDebug() << "JsonSaveDir created successfully";

    QFile file(qstrJsonSaveDir + "/" + filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file for writing.";
    }

    file.write(jsonDoc.toJson()); // 将JSON文档转换为字节数组并写入文件
    file.close();

    qDebug() << "JSON data has been written to file.";
}

int ImageProcAlgo::Proc_DirectPy()
{
    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;
    m_pGParam->m_SImageProcessorData.bFullLEO = (*iter).fExpTime < 500.0;
    int iTime1, iTime2, iTime3, iTime4, iTime5, iTime6, iTime7, iTime8, iTime9, iTime10, iTime11, iTime12, iTime13;
    if (m_cmRotate != NULL)
    {
        /// Generate Image Photometry
        ClockOn();
        (*iter).safImagePhotometry.pvoidBuffer = (void*)new float[m_szImageWidth * m_szImageHeight];
        float* pafImagePhotometry = (float*)((*iter).safImagePhotometry.pvoidBuffer);
        memset(pafImagePhotometry, 0, m_szImageWidth * m_szImageHeight * sizeof(float));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmPhotometry, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(float), pafImagePhotometry, 0, NULL, NULL);
        iTime1 = ClockOff();
        qDebug() << "### Write Image for Photometry:" << iTime1 << "ms";

        /// Avg,Std
        ClockOn();
        double dAvg, dStd, dAvg1, dStd1, dAvg2, dStd2, dAvg3, dStd3, dAvg4, dStd4, dAvg5, dStd5;
        AvgStdGPU16(m_cmRemoveBadLine, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
        (*iter).sausImageGrab.dAvg = dAvg;
        (*iter).sausImageGrab.dStd = dStd;
        iTime2 = ClockOff();
        qDebug() << "### Proc AvgStd:" << iTime2 << "ms";

        /// Median Filter
        ClockOn();
        MedianFilter16(m_cmRemoveBadLine, m_cmMedian, m_szImageWidth, m_szImageHeight, 3);  // m_cmMedian only used to display
        iTime3 = ClockOff();
        qDebug() << "### Proc Median:" << iTime3 << "ms";

        /// ReduceSmallDN
        ClockOn();
        if ((m_szImageWidth==1024 && m_szImageHeight==1024))
        {
            ReduceSmallDN16(m_cmMedian, m_cmEnhancePre, (*iter).sausImageGrab.dAvg, (*iter).sausImageGrab.dStd, m_szImageWidth, m_szImageHeight);
        }
        else if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            for (int r = 0; r < 6; r++)
            {
                for (int c = 0; c < 6; c++)
                {
                    /// Patch
                    Patch16(m_cmMedian, m_cmPitchEnhance, r, c);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance, m_cmPitchEnhance1, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg2, &dStd2, 1024, 1024);

                    /// ReduceSmallDN
                    ReduceSmallDN16(m_cmPitchEnhance, m_cmPitchEnhance5, dAvg2, dStd2, 1024, 1024);

                    /// DePatch
                    DePatch16(m_cmPitchEnhance5, m_cmEnhancePre, r, c);
                }
            }
        }
        iTime4 = ClockOff();
        qDebug() << "### Proc ReduceSmallDN:" << iTime4 << "ms";

//        /// LCM
//        ClockOn();
//        LocalContrastMeasure(m_cmEnhancePre, m_cmNoStar, 3, m_szImageWidth, m_szImageHeight);
//        iTime5 = ClockOff();
//        qDebug() << "### Proc LCM:" << iTime5 << "ms";
//        /// Erode Gray
//        ErodeGray(m_cmNoStar, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);

        /// LCM
        ClockOn();
        if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 5, m_szImageWidth, m_szImageHeight);
//            LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);
        }
        else if ((m_szImageWidth==1024 && m_szImageHeight==1024))
        {
            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmEnhancePre, m_cmEnhance, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), 0, NULL, NULL);
        }
//        LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);
        iTime5 = ClockOff();
        qDebug() << "### Proc LCM:" << iTime5 << "ms";

        /// Generate Image Enhance
        ClockOn();
        (*iter).sausImageEnhance.pvoidBuffer = (void*)new unsigned short[m_szImageWidth * m_szImageHeight];
        unsigned short* pausImageEnhance = (unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
        memset(pausImageEnhance, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmMedian, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausImageEnhance, 0, NULL, NULL);
        iTime6 = ClockOff();
        qDebug() << "### Generate Image Enhance:" << iTime6 << "ms";

        /// BW
        //****************************Pitch BW****************************//
        ClockOn();
        if ((m_szImageWidth==1024 && m_szImageHeight==1024)
                || m_pGParam->m_SImageProcessorData.bFullLEO)
        {
            /// Avg,Std
            AvgStdGPU16(m_cmEnhance, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance, m_cmEnhance1, dAvg+3.0*dStd, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance1, &dAvg1, &dStd1, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance1, m_cmEnhance2, dAvg1+3.0*dStd1, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance2, &dAvg2, &dStd2, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance2, m_cmEnhance3, dAvg2+3.0*dStd2, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance3, &dAvg3, &dStd3, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance3, m_cmEnhance4, dAvg3+3.0*dStd3, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance4, &dAvg4, &dStd4, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance4, m_cmEnhance5, dAvg4+3.0*dStd4, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance5, &dAvg5, &dStd5, m_szImageWidth, m_szImageHeight);
            (*iter).sausImageEnhance.dAvg = dAvg5;
            (*iter).sausImageEnhance.dStd = dStd5;

            /// BW
            Binary16(m_cmEnhance, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg5 + m_fRatioStd * dStd5));
        }
        else if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            (*iter).sausImageEnhance.dAvg = 0;
            (*iter).sausImageEnhance.dStd = 0;
            for (int r = 0; r < 6; r++)
            {
                for (int c = 0; c < 6; c++)
                {
                    /// Patch
                    Patch16(m_cmEnhance, m_cmPitchEnhance, r, c);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance, m_cmPitchEnhance1, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg2, &dStd2, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance2, m_cmPitchEnhance3, dAvg2+3.0*dStd2, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance3, &dAvg3, &dStd3, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance3, m_cmPitchEnhance4, dAvg3+3.0*dStd3, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance4, &dAvg4, &dStd4, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance4, m_cmPitchEnhance5, dAvg4+3.0*dStd4, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance5, &dAvg5, &dStd5, 1024, 1024);
//                    (*iter).sausImageEnhance.dAvg += dAvg5;
//                    (*iter).sausImageEnhance.dStd += dStd5;

                    /// Gaussian Filter
                    SubBG16(m_cmPitchEnhance, m_cmPitchEnhance1, 5.0, dAvg5, dStd5, 1.0, 1024, 1024);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance2, m_cmPitchEnhance3, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance3, &dAvg2, &dStd2, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance3, m_cmPitchEnhance4, dAvg2+3.0*dStd2, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance4, &dAvg3, &dStd3, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance4, m_cmPitchEnhance5, dAvg3+3.0*dStd3, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance5, &dAvg4, &dStd4, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance5, m_cmPitchEnhance, dAvg4+3.0*dStd4, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg5, &dStd5, 1024, 1024);
                    (*iter).sausImageEnhance.dAvg += dAvg5;
                    (*iter).sausImageEnhance.dStd += dStd5;

                    /// BW
//                    Binary16(m_cmPitchEnhance, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd-1.0:m_fRatioStd) * dStd5));
//                    Binary16(m_cmPitchEnhance1, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd-1.0:m_fRatioStd) * dStd5));
                    Binary16(m_cmPitchEnhance1, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd:m_fRatioStd) * dStd5));

                    /// DePatch
                    DePatch8(m_cmPitchNoStarBW, m_cmNoStarBW2, r, c);
                    /// Erode
                    ErodeBW(m_cmNoStarBW2, m_cmNoStarBW, 3, m_szImageWidth, m_szImageHeight);
//                    /// DePatch
//                    DePatch8(m_cmPitchNoStarBW, m_cmNoStarBW, r, c);
                }
            }
            (*iter).sausImageEnhance.dAvg /= 36.0;
            (*iter).sausImageEnhance.dStd /= 36.0;
        }
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
        iTime7 = ClockOff();
        qDebug() << "### Proc Binary:" << iTime7 << "ms";
        //***********************************************************************//

        /// Blob
        ClockOn();
        if (!m_blobs.empty())
        {
            m_pLabeler->ReleaseBlobs(m_blobs);
        }
//        m_pLabeler->LabelEff(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
        m_pLabeler->Label(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);

        m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
        RemoveBadPixel(m_blobs);
        RemoveBadBlob(m_blobs);
        m_iNumBlob = m_blobs.size();

        /// Centroid
        /// Calculate Point Error
        float fFOVCenterAzi = (*iter).pairfFOVCenterAE.first;
        float fFOVCenterEle = (*iter).pairfFOVCenterAE.second;
        double dPointErrAzi = 0;
        double dPointErrEle = 0;
        CalcPointError(fFOVCenterAzi, fFOVCenterEle, dPointErrAzi, dPointErrEle);
        /// FOV Center Azimuth and Elevation Modify
        float fFOVCenterAziModify = fFOVCenterAzi - dPointErrAzi;
        float fFOVCenterEleModify = fFOVCenterEle - dPointErrEle;
        (*iter).pairfFOVCenterAEModify.first = fFOVCenterAziModify;
        (*iter).pairfFOVCenterAEModify.second = fFOVCenterEleModify;

        WriteJsonFile(false);


        vector<sMeasureBlob> vectTargetMeasures;
        if (m_blobs.size() > 0/* && m_blobs.size() < 1000*/)
        {
//            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 5, 1.0, 1.0);
//            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 2, 1.0, 1.0);
            int lastDotIndex = m_pGParam->m_SImageReplayerData.qstrCurFileName.lastIndexOf('/');
            QString qstrInputSaveDir = m_pGParam->m_SImageReplayerData.qstrCurFileName.left(lastDotIndex) + "/Input", filenameInput = m_pGParam->m_SImageReplayerData.qstrCurFileName.mid(lastDotIndex + 1) + "Input.csv";
            QDir qdir;
            if (!qdir.exists(qstrInputSaveDir))
                if (qdir.mkpath(qstrInputSaveDir))
                    qDebug() << "InputSaveDir created successfully";
            QFile file(qstrInputSaveDir + "/" + filenameInput);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                qDebug() << "Could not open file for writing.";
            QList<QStringList> rowDataList;
            int blobId = 0;

            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 0, 1.0, 1.0);
            for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
            {
                sMeasureBlob blob;
                double dJBDx = 0.0, dJBDy = 0.0;
                blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
                CalcDistortionDelta(blob.pairfPos.first, blob.pairfPos.second, dJBDx, dJBDy);
                blob.pairfPos.first -= dJBDx;
                blob.pairfPos.second -= dJBDy;
                blob.fMinX = (*iterB).second->minx * m_iBinning;
                blob.fMinY = (*iterB).second->miny * m_iBinning;
                blob.fMaxX = (*iterB).second->maxx * m_iBinning;
                blob.fMaxY = (*iterB).second->maxy * m_iBinning;
                blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
                blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿

                //****************************************************************//

                /// Calculate Target Azimuth and Elevation
                float fDistAzi = (blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale / cos(fFOVCenterEleModify / 180.0 * 3.1415926);
                float fDistEle = (m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale;
                float fTargetAzi = fFOVCenterAziModify + fDistAzi;
                float fTargetEle = fFOVCenterEleModify + fDistEle;

                blob.fFOVCenterAziModify = fFOVCenterAziModify;
                blob.fFOVCenterEleModify = fFOVCenterEleModify;
                blob.fDistAzi = fDistAzi;
                blob.fDistEle = fDistEle;
                blob.fTargetAzi = fTargetAzi;
                blob.fTargetEle = fTargetEle;
                blob.dPointErrEle = dPointErrEle;

                int iYear, iMonth, iDay, iHour;
                double dRa = 0.0, dDe = 0.0;
                double dRm = 0.0, dDm = 0.0;
                double dA = blob.pairfPosAE.first * pi / 180;
                double dE = blob.pairfPosAE.second * pi / 180;
                double dLatitude = m_pGParam->m_SObsParams.fLatitude * pi / 180;
                double dLongitude = m_pGParam->m_SObsParams.fLongitude * pi / 180;
                BJ2UTC((*iter).stimeFrame.iYear,
                        (*iter).stimeFrame.iMonth,
                        (*iter).stimeFrame.iDay,
                        (*iter).stimeFrame.iHour,
                        iYear, iMonth, iDay, iHour);
                m_pTrakcer->AangleToEquator(dA, dE,
                                dLongitude, dLatitude, iYear, iMonth, iDay,
                                iHour, (*iter).stimeFrame.iMinute,
                                (double)(*iter).stimeFrame.iSecond + (*iter).stimeFrame.iMillisecond/1000.0,
                                (*iter).fAtmosP/ 100.0, (*iter).fTemp + 273, true, &dRa, &dDe, &dRm, &dDm, true);
                blob.dAlpha = dRm;
                blob.dSigma = dDm;

                blob.dRa = blob.dAlpha;
                blob.dDec = blob.dSigma;

                QStringList rowData = {QString::number(blobId), QString::fromStdString(std::to_string(blob.pairfPos.first)), QString::fromStdString(std::to_string(blob.pairfPos.second))
                                      , "", ""};
                rowDataList.push_back(rowData);
                m_umapBlob.insert(std::make_pair(std::to_string(blobId), blob));
//                if (!m_pGParam->m_SImageProcessorData.bFullLEO || (m_szImageWidth==m_pGParam->m_SGrabberData.iSubWidth && m_szImageHeight==m_pGParam->m_SGrabberData.iSubHeight))
//                    vectTargetMeasures.push_back(blob);
//                else if (blob.pairfPos.first >= 1024 && blob.pairfPos.first <= 6144-1024
//                         && blob.pairfPos.second >= 1024 && blob.pairfPos.second <= 6144-1024)
//                    vectTargetMeasures.push_back(blob);
                vectTargetMeasures.push_back(blob);
                blobId++;
                //****************************************************************//
            }

            QTextStream stream(&file);
            for (const QStringList &rowData : rowDataList)
            {
                    for (int i = 0; i < rowData.size(); ++i)
                    {
                        stream << rowData[i];
                        if (i < rowData.size() - 1)
                            stream << ",";
                    }
                    stream << "\n"; // 换行表示新的行
            }
            file.close();
            qDebug() << "Data has been written to CSV file.";

            QString pythonInterpreter = "/home/kk/anaconda3/envs/env_twdw_gdcl/bin/python";
            QString pythonScript = "/home/kk/code/Python_Project/1mZGG/Source/solve.py"; // Python 脚本路径
            QStringList functionArguments;
            functionArguments << m_pGParam->m_SImageReplayerData.qstrCurFileName; // 函数参数，比如文件路径

            QProcess pythonProcess;
            pythonProcess.start(pythonInterpreter, QStringList() << pythonScript << functionArguments);

            if (!pythonProcess.waitForStarted())
                qDebug() << "Failed to start Python process.";
            if (!pythonProcess.waitForFinished())
                qDebug() << "Failed to finish Python process.";
            QString output = pythonProcess.readAllStandardOutput();
            qDebug() << "Python output:" << output;
        }
    }
}

int ImageProcAlgo::Proc_Direct(void)
{
    vector<sFramePacket>::iterator iter = m_vectFramePacket.end() - 1;
    m_pGParam->m_SImageProcessorData.bFullLEO = (*iter).fExpTime < 500.0;
    int iTime1, iTime2, iTime3, iTime4, iTime5, iTime6, iTime7, iTime8, iTime9, iTime10, iTime11, iTime12, iTime13;
    if (m_cmRotate != NULL)
    {
        /// Generate Image Photometry
        ClockOn();
        (*iter).safImagePhotometry.pvoidBuffer = (void*)new float[m_szImageWidth * m_szImageHeight];
        float* pafImagePhotometry = (float*)((*iter).safImagePhotometry.pvoidBuffer);
        memset(pafImagePhotometry, 0, m_szImageWidth * m_szImageHeight * sizeof(float));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmPhotometry, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(float), pafImagePhotometry, 0, NULL, NULL);
        iTime1 = ClockOff();
        qDebug() << "### Write Image for Photometry:" << iTime1 << "ms";

        /// Avg,Std
        ClockOn();
        double dAvg, dStd, dAvg1, dStd1, dAvg2, dStd2, dAvg3, dStd3, dAvg4, dStd4, dAvg5, dStd5;
        AvgStdGPU16(m_cmRemoveBadLine, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
        (*iter).sausImageGrab.dAvg = dAvg;
        (*iter).sausImageGrab.dStd = dStd;
        iTime2 = ClockOff();
        qDebug() << "### Proc AvgStd:" << iTime2 << "ms";

        /// Median Filter
        ClockOn();
        MedianFilter16(m_cmRemoveBadLine, m_cmMedian, m_szImageWidth, m_szImageHeight, 3);  // m_cmMedian only used to display
        iTime3 = ClockOff();
        qDebug() << "### Proc Median:" << iTime3 << "ms";

        /// ReduceSmallDN
        ClockOn();
        if ((m_szImageWidth==1024 && m_szImageHeight==1024))
        {
            ReduceSmallDN16(m_cmMedian, m_cmEnhancePre, (*iter).sausImageGrab.dAvg, (*iter).sausImageGrab.dStd, m_szImageWidth, m_szImageHeight);
        }
        else if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            for (int r = 0; r < 6; r++)
            {
                for (int c = 0; c < 6; c++)
                {
                    /// Patch
                    Patch16(m_cmMedian, m_cmPitchEnhance, r, c);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance, m_cmPitchEnhance1, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg2, &dStd2, 1024, 1024);

                    /// ReduceSmallDN
                    ReduceSmallDN16(m_cmPitchEnhance, m_cmPitchEnhance5, dAvg2, dStd2, 1024, 1024);

                    /// DePatch
                    DePatch16(m_cmPitchEnhance5, m_cmEnhancePre, r, c);
                }
            }
        }
        iTime4 = ClockOff();
        qDebug() << "### Proc ReduceSmallDN:" << iTime4 << "ms";

//        /// LCM
//        ClockOn();
//        LocalContrastMeasure(m_cmEnhancePre, m_cmNoStar, 3, m_szImageWidth, m_szImageHeight);
//        iTime5 = ClockOff();
//        qDebug() << "### Proc LCM:" << iTime5 << "ms";
//        /// Erode Gray
//        ErodeGray(m_cmNoStar, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);

        /// LCM
        ClockOn();
        if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 5, m_szImageWidth, m_szImageHeight);
//            LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);
        }
        else if ((m_szImageWidth==1024 && m_szImageHeight==1024))
        {
            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmEnhancePre, m_cmEnhance, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), 0, NULL, NULL);
        }
//        LocalContrastMeasure(m_cmEnhancePre, m_cmEnhance, 3, m_szImageWidth, m_szImageHeight);
        iTime5 = ClockOff();
        qDebug() << "### Proc LCM:" << iTime5 << "ms";

        /// Generate Image Enhance
        ClockOn();
        (*iter).sausImageEnhance.pvoidBuffer = (void*)new unsigned short[m_szImageWidth * m_szImageHeight];
        unsigned short* pausImageEnhance = (unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
        memset(pausImageEnhance, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short));
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmMedian, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausImageEnhance, 0, NULL, NULL);
        iTime6 = ClockOff();
        qDebug() << "### Generate Image Enhance:" << iTime6 << "ms";

        /// BW
        //****************************Pitch BW****************************//
        ClockOn();
        if ((m_szImageWidth==1024 && m_szImageHeight==1024)
                || m_pGParam->m_SImageProcessorData.bFullLEO)
        {
            /// Avg,Std
            AvgStdGPU16(m_cmEnhance, &dAvg, &dStd, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance, m_cmEnhance1, dAvg+3.0*dStd, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance1, &dAvg1, &dStd1, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance1, m_cmEnhance2, dAvg1+3.0*dStd1, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance2, &dAvg2, &dStd2, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance2, m_cmEnhance3, dAvg2+3.0*dStd2, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance3, &dAvg3, &dStd3, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance3, m_cmEnhance4, dAvg3+3.0*dStd3, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance4, &dAvg4, &dStd4, m_szImageWidth, m_szImageHeight);
            ReduceLargeDN16(m_cmEnhance4, m_cmEnhance5, dAvg4+3.0*dStd4, m_szImageWidth, m_szImageHeight);
            AvgStdGPU16(m_cmEnhance5, &dAvg5, &dStd5, m_szImageWidth, m_szImageHeight);
            (*iter).sausImageEnhance.dAvg = dAvg5;
            (*iter).sausImageEnhance.dStd = dStd5;

            /// BW
            Binary16(m_cmEnhance, m_cmNoStarBW, m_szImageWidth, m_szImageHeight, (unsigned short)(dAvg5 + m_fRatioStd * dStd5));
        }
        else if (m_szImageWidth==6144 && m_szImageHeight==6144)
        {
            (*iter).sausImageEnhance.dAvg = 0;
            (*iter).sausImageEnhance.dStd = 0;
            for (int r = 0; r < 6; r++)
            {
                for (int c = 0; c < 6; c++)
                {
                    /// Patch
                    Patch16(m_cmEnhance, m_cmPitchEnhance, r, c);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance, m_cmPitchEnhance1, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg2, &dStd2, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance2, m_cmPitchEnhance3, dAvg2+3.0*dStd2, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance3, &dAvg3, &dStd3, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance3, m_cmPitchEnhance4, dAvg3+3.0*dStd3, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance4, &dAvg4, &dStd4, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance4, m_cmPitchEnhance5, dAvg4+3.0*dStd4, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance5, &dAvg5, &dStd5, 1024, 1024);
//                    (*iter).sausImageEnhance.dAvg += dAvg5;
//                    (*iter).sausImageEnhance.dStd += dStd5;

                    /// Gaussian Filter
                    SubBG16(m_cmPitchEnhance, m_cmPitchEnhance1, 5.0, dAvg5, dStd5, 1.0, 1024, 1024);

                    /// Avg,Std
                    AvgStdGPU16(m_cmPitchEnhance1, &dAvg, &dStd, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance1, m_cmPitchEnhance2, dAvg+3.0*dStd, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance2, &dAvg1, &dStd1, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance2, m_cmPitchEnhance3, dAvg1+3.0*dStd1, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance3, &dAvg2, &dStd2, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance3, m_cmPitchEnhance4, dAvg2+3.0*dStd2, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance4, &dAvg3, &dStd3, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance4, m_cmPitchEnhance5, dAvg3+3.0*dStd3, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance5, &dAvg4, &dStd4, 1024, 1024);
                    ReduceLargeDN16(m_cmPitchEnhance5, m_cmPitchEnhance, dAvg4+3.0*dStd4, 1024, 1024);
                    AvgStdGPU16(m_cmPitchEnhance, &dAvg5, &dStd5, 1024, 1024);
                    (*iter).sausImageEnhance.dAvg += dAvg5;
                    (*iter).sausImageEnhance.dStd += dStd5;

                    /// BW
//                    Binary16(m_cmPitchEnhance, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd-1.0:m_fRatioStd) * dStd5));
//                    Binary16(m_cmPitchEnhance1, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd-1.0:m_fRatioStd) * dStd5));
                    Binary16(m_cmPitchEnhance1, m_cmPitchNoStarBW, 1024, 1024, (unsigned short)(dAvg5 + ((r==0||c==0||r==5||c==5)?m_fRatioStd:m_fRatioStd) * dStd5));

                    /// DePatch
                    DePatch8(m_cmPitchNoStarBW, m_cmNoStarBW2, r, c);
                    /// Erode
                    ErodeBW(m_cmNoStarBW2, m_cmNoStarBW, 3, m_szImageWidth, m_szImageHeight);
//                    /// DePatch
//                    DePatch8(m_cmPitchNoStarBW, m_cmNoStarBW, r, c);
                }
            }
            (*iter).sausImageEnhance.dAvg /= 36.0;
            (*iter).sausImageEnhance.dStd /= 36.0;
        }
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucNoStarBW, 0, NULL, NULL);
        iTime7 = ClockOff();
        qDebug() << "### Proc Binary:" << iTime7 << "ms";
        //***********************************************************************//

        /// Blob
        ClockOn();
        if (!m_blobs.empty())
        {
            m_pLabeler->ReleaseBlobs(m_blobs);
        }
//        m_pLabeler->LabelEff(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
        m_pLabeler->Label(m_paucNoStarBW, m_szImageWidth, m_szImageHeight, m_blobs, 0, 0, m_szImageWidth, m_szImageHeight);
//        for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
//        {
//            sMeasureBlob blob;
//            blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
//            blob.fMinX = (*iterB).second->minx * m_iBinning;
//            blob.fMinY = (*iterB).second->miny * m_iBinning;
//            blob.fMaxX = (*iterB).second->maxx * m_iBinning;
//            blob.fMaxY = (*iterB).second->maxy * m_iBinning;
//            blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
//            blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
//            blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
//        }
        m_pLabeler->FilterByArea(m_blobs, m_iMinArea, m_iMaxArea);
        RemoveBadPixel(m_blobs);
        RemoveBadBlob(m_blobs);
        m_iNumBlob = m_blobs.size();

        /// Centroid
        /// Calculate Point Error
        float fFOVCenterAzi = (*iter).pairfFOVCenterAE.first;
        float fFOVCenterEle = (*iter).pairfFOVCenterAE.second;
        double dPointErrAzi = 0;
        double dPointErrEle = 0;
        CalcPointError(fFOVCenterAzi, fFOVCenterEle, dPointErrAzi, dPointErrEle);
        /// FOV Center Azimuth and Elevation Modify
        float fFOVCenterAziModify = fFOVCenterAzi - dPointErrAzi;
        float fFOVCenterEleModify = fFOVCenterEle - dPointErrEle;
        (*iter).pairfFOVCenterAEModify.first = fFOVCenterAziModify;
        (*iter).pairfFOVCenterAEModify.second = fFOVCenterEleModify;

        WriteJsonFile(false);

        vector<sMeasureBlob> vectTargetMeasures;
        if (m_blobs.size() > 0/* && m_blobs.size() < 1000*/)
        {            
//            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 5, 1.0, 1.0);
//            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 2, 1.0, 1.0);
            m_pLabeler->Centroids(pausImageEnhance, m_szImageWidth, m_szImageHeight, m_blobs, 0, 1.0, 1.0);
            for (Blobs::iterator iterB = m_blobs.begin(); iterB != m_blobs.end(); iterB++)
            {
                sMeasureBlob blob;
                double dJBDx = 0.0, dJBDy = 0.0;
                blob.pairfPos = pair<float, float>((*iterB).second->cx * m_iBinning, (*iterB).second->cy * m_iBinning); // cx,cy: 质心;centroid_x,centroid_y: 形心
                CalcDistortionDelta(blob.pairfPos.first, blob.pairfPos.second, dJBDx, dJBDy);
                blob.pairfPos.first -= dJBDx;
                blob.pairfPos.second -= dJBDy;
                blob.fMinX = (*iterB).second->minx * m_iBinning;
                blob.fMinY = (*iterB).second->miny * m_iBinning;
                blob.fMaxX = (*iterB).second->maxx * m_iBinning;
                blob.fMaxY = (*iterB).second->maxy * m_iBinning;
                blob.fArea = (*iterB).second->area * m_iBinning * m_iBinning;
                blob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                blob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿

                //****************************************************************//

                /// Calculate Target Azimuth and Elevation
                float fDistAzi = (blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale / cos(fFOVCenterEleModify / 180.0 * 3.1415926);
                float fDistEle = (m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale;
                float fTargetAzi = fFOVCenterAziModify + fDistAzi;
                float fTargetEle = fFOVCenterEleModify + fDistEle;

                blob.fFOVCenterAziModify = fFOVCenterAziModify;
                blob.fFOVCenterEleModify = fFOVCenterEleModify;
                blob.fDistAzi = fDistAzi;
                blob.fDistEle = fDistEle;
                blob.fTargetAzi = fTargetAzi;
                blob.fTargetEle = fTargetEle;
                blob.dPointErrEle = dPointErrEle;

                int iYear, iMonth, iDay, iHour;
                double dRa = 0.0, dDe = 0.0;
                double dRm = 0.0, dDm = 0.0;
                double dA = blob.pairfPosAE.first * pi / 180;
                double dE = blob.pairfPosAE.second * pi / 180;
                double dLatitude = m_pGParam->m_SObsParams.fLatitude * pi / 180;
                double dLongitude = m_pGParam->m_SObsParams.fLongitude * pi / 180;
                BJ2UTC((*iter).stimeFrame.iYear,
                        (*iter).stimeFrame.iMonth,
                        (*iter).stimeFrame.iDay,
                        (*iter).stimeFrame.iHour,
                        iYear, iMonth, iDay, iHour);
                m_pTrakcer->AangleToEquator(dA, dE,
                                dLongitude, dLatitude, iYear, iMonth, iDay,
                                iHour, (*iter).stimeFrame.iMinute,
                                (double)(*iter).stimeFrame.iSecond + (*iter).stimeFrame.iMillisecond/1000.0,
                                (*iter).fAtmosP/ 100.0, (*iter).fTemp + 273, true, &dRa, &dDe, &dRm, &dDm, true);
                blob.dAlpha = dRm;
                blob.dSigma = dDm;

                blob.dRa = blob.dAlpha;
                blob.dDec = blob.dSigma;
//                if (!m_pGParam->m_SImageProcessorData.bFullLEO || (m_szImageWidth==m_pGParam->m_SGrabberData.iSubWidth && m_szImageHeight==m_pGParam->m_SGrabberData.iSubHeight))
//                    vectTargetMeasures.push_back(blob);
//                else if (blob.pairfPos.first >= 1024 && blob.pairfPos.first <= 6144-1024
//                         && blob.pairfPos.second >= 1024 && blob.pairfPos.second <= 6144-1024)
//                    vectTargetMeasures.push_back(blob);
                vectTargetMeasures.push_back(blob);

                //****************************************************************//
            }
        }

        sMeasureBlob ManualBlob;
        if(m_iTrackMode == TRACK_MANUAL)
        {
            if(m_pGParam->m_SImageProcessorData.bDoubleClicked)
            {
                double dJBDx = 0.0, dJBDy = 0.0;
                ManualBlob.pairfPos = m_pGParam->m_SImageProcessorData.pairfClickedPos; // cx,cy: 质心;centroid_x,centroid_y: 形心
                CalcDistortionDelta(ManualBlob.pairfPos.first, ManualBlob.pairfPos.second, dJBDx, dJBDy);
                ManualBlob.pairfPos.first -= dJBDx;
                ManualBlob.pairfPos.second -= dJBDy;
                ManualBlob.fMaxX = ManualBlob.pairfPos.first + 10;
                ManualBlob.fMaxY = ManualBlob.pairfPos.second + 10;
                ManualBlob.fMinX = ManualBlob.pairfPos.first - 10;
                ManualBlob.fMinY = ManualBlob.pairfPos.second - 10;
                ManualBlob.fArea = 100;
                ManualBlob.fDN = 10000;
                ManualBlob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - ManualBlob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
                ManualBlob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (ManualBlob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(ManualBlob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿

                //****************************************************************//
                /// Calculate Target Azimuth and Elevation
                float fDistAzi = (ManualBlob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale / cos(fFOVCenterEleModify / 180.0 * 3.1415926);
                float fDistEle = (m_pGParam->m_SOpticData.fOptCenterY - ManualBlob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale;
                float fTargetAzi = fFOVCenterAziModify + fDistAzi;
                float fTargetEle = fFOVCenterEleModify + fDistEle;

                ManualBlob.fFOVCenterAziModify = fFOVCenterAziModify;
                ManualBlob.fFOVCenterEleModify = fFOVCenterEleModify;
                ManualBlob.fDistAzi = fDistAzi;
                ManualBlob.fDistEle = fDistEle;
                ManualBlob.fTargetAzi = fTargetAzi;
                ManualBlob.fTargetEle = fTargetEle;
                ManualBlob.dPointErrEle = dPointErrEle;

                int iYear, iMonth, iDay, iHour;
                double dRa = 0.0, dDe = 0.0;
                double dRm = 0.0, dDm = 0.0;
                double dA = ManualBlob.pairfPosAE.first * pi / 180;
                double dE = ManualBlob.pairfPosAE.second * pi / 180;
                double dLatitude = m_pGParam->m_SObsParams.fLatitude * pi / 180;
                double dLongitude = m_pGParam->m_SObsParams.fLongitude * pi / 180;
                BJ2UTC((*iter).stimeFrame.iYear,
                        (*iter).stimeFrame.iMonth,
                        (*iter).stimeFrame.iDay,
                        (*iter).stimeFrame.iHour,
                        iYear, iMonth, iDay, iHour);
                m_pTrakcer->AangleToEquator(dA, dE,
                                dLongitude, dLatitude, iYear, iMonth, iDay,
                                iHour, (*iter).stimeFrame.iMinute,
                                (double)(*iter).stimeFrame.iSecond + (*iter).stimeFrame.iMillisecond/1000.0,
                                (*iter).fAtmosP/ 100.0, (*iter).fTemp + 273, true, &dRa, &dDe, &dRm, &dDm, true);
                ManualBlob.dAlpha = dRm;
                ManualBlob.dSigma = dDm;

                ManualBlob.dRa = ManualBlob.dAlpha;
                ManualBlob.dDec = ManualBlob.dSigma;
            }
        }

        vector<pair<QString, sMeasureBlob>> vecGEOisbValidBlob;
        if (m_iTrackMode == TRACK_GEO)
        {
            for(int i = 0; i < m_vectTargetInfo.size(); i++)
            {
                sMeasureBlob GEOBlob;
                double dJBDx = 0.0, dJBDy = 0.0;
                GEOBlob.pairfPos = m_vectTargetInfo[i].pairfPredPosInFrame; // cx,cy: 质心;centroid_x,centroid_y: 形心
//                CalcDistortionDelta(GEOBlob.pairfPos.first, GEOBlob.pairfPos.second, dJBDx, dJBDy);
//                GEOBlob.pairfPos.first -= dJBDx;
//                GEOBlob.pairfPos.second -= dJBDy;
                GEOBlob.fMaxX = GEOBlob.pairfPos.first + 5;
                GEOBlob.fMaxY = GEOBlob.pairfPos.second + 5;
                GEOBlob.fMinX = GEOBlob.pairfPos.first - 5;
                GEOBlob.fMinY = GEOBlob.pairfPos.second - 5;
                GEOBlob.fArea = 0;
                GEOBlob.fDN = 0;
//                GEOBlob.pairfPosAE.second = (*iter).pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - GEOBlob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
//                GEOBlob.pairfPosAE.first = (*iter).pairfFOVCenterAE.first + (GEOBlob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(GEOBlob.pairfPosAE.second / 180.0 * 3.1415926);	// 已正割补偿
                GEOBlob.pairfPosAE = m_vectTargetInfo[i].pairfPredPosAE;

                //****************************************************************//
                /// Calculate Target Azimuth and Elevation
                float fDistAzi = (GEOBlob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale / cos(fFOVCenterEleModify / 180.0 * 3.1415926);
                float fDistEle = (m_pGParam->m_SOpticData.fOptCenterY - GEOBlob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale;
                float fTargetAzi = fFOVCenterAziModify + fDistAzi;
                float fTargetEle = fFOVCenterEleModify + fDistEle;

                GEOBlob.fFOVCenterAziModify = fFOVCenterAziModify;
                GEOBlob.fFOVCenterEleModify = fFOVCenterEleModify;
                GEOBlob.fDistAzi = fDistAzi;
                GEOBlob.fDistEle = fDistEle;
                GEOBlob.fTargetAzi = fTargetAzi;
                GEOBlob.fTargetEle = fTargetEle;
                GEOBlob.dPointErrEle = dPointErrEle;

                int iYear, iMonth, iDay, iHour;
                double dRa = 0.0, dDe = 0.0;
                double dRm = 0.0, dDm = 0.0;
                double dA = GEOBlob.pairfPosAE.first * pi / 180;
                double dE = GEOBlob.pairfPosAE.second * pi / 180;
                double dLatitude = m_pGParam->m_SObsParams.fLatitude * pi / 180;
                double dLongitude = m_pGParam->m_SObsParams.fLongitude * pi / 180;
                BJ2UTC((*iter).stimeFrame.iYear,
                        (*iter).stimeFrame.iMonth,
                        (*iter).stimeFrame.iDay,
                        (*iter).stimeFrame.iHour,
                        iYear, iMonth, iDay, iHour);
                m_pTrakcer->AangleToEquator(dA, dE,
                                dLongitude, dLatitude, iYear, iMonth, iDay,
                                iHour, (*iter).stimeFrame.iMinute,
                                (double)(*iter).stimeFrame.iSecond + (*iter).stimeFrame.iMillisecond/1000.0,
                                (*iter).fAtmosP/ 100.0, (*iter).fTemp + 273, true, &dRa, &dDe, &dRm, &dDm, true);
                GEOBlob.dAlpha = dRm;
                GEOBlob.dSigma = dDm;

                GEOBlob.dRa = m_vectTargetInfo[i].pairfPredPosInFrame.first;
                GEOBlob.dDec = m_vectTargetInfo[i].pairfPredPosInFrame.second;

                vecGEOisbValidBlob.push_back(make_pair(m_vectTargetInfo[i].qstrTargetID, GEOBlob));
            }
        }

        /// 向帧数据内添加恒星位置数据
        (*iter).vectStarMeasures = vectTargetMeasures;
        /// 测量数据打包
        sMeasuresInFrame measuresTarget;
        measuresTarget.fExpTime = (*iter).fExpTime;
        measuresTarget.fFrameFreq = (*iter).fFrameFreq;
        measuresTarget.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
        measuresTarget.stimeFrame = (*iter).stimeFrame;
        measuresTarget.ulFrameSeq = (*iter).ulFrameSeq;
        measuresTarget.dTemp = (*iter).fTemp;
        measuresTarget.dAtmosP = (*iter).fAtmosP;
        measuresTarget.vectTargetMeasures = vectTargetMeasures;

        iTime8 = ClockOff();
        qDebug() << "### Proc Blob:" << iTime8 << "ms";

        /// 天文定位数据整形+指向误差计算
        ClockOn();
        if (m_iTrackMode != TRACK_SC && m_iTrackMode != TRACK_LEO)
        {
            TWDataDeal((*iter).ulFrameSeq);
        }
        iTime9 = ClockOff();
        qDebug() << "### Proc TWDWData Deal:" << iTime9 << "ms";

        /// 计算恒星运动速度
        ClockOn();
        if (m_iTrackMode == TRACK_GEO)
        {
            sMeasuresInFrame measuresStar;
            measuresStar.fExpTime = (*iter).fExpTime;
            measuresStar.fFrameFreq = (*iter).fFrameFreq;
            measuresStar.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
            measuresStar.stimeFrame = (*iter).stimeFrame;
            measuresStar.ulFrameSeq = (*iter).ulFrameSeq;

            int iNumStar = (*iter).vectStarMeasures.size();

            float fRatioFOV = (500 / (float)iNumStar);
            fRatioFOV = fRatioFOV >= 1 ? 1 : fRatioFOV;

            size_t szCropWidth = (size_t)(m_szGrabWidth * pow(fRatioFOV, 0.5));
            size_t szCropHeight = (size_t)(m_szGrabHeight * pow(fRatioFOV, 0.5));

            /// 兼容短曝光全帧目标跟踪,恒星搜索范围变大
            if (m_pGParam->m_SImageProcessorData.bFullLEO)
            {
                szCropWidth = 4096;
                szCropHeight = 4096;
            }

            for (int i = 0; i < (*iter).vectStarMeasures.size(); i++)
            {
                if ((*iter).vectStarMeasures[i].pairfPos.first > m_pGParam->m_SOpticData.fOptCenterX - szCropWidth * 0.5
                        && (*iter).vectStarMeasures[i].pairfPos.first < m_pGParam->m_SOpticData.fOptCenterX + szCropWidth * 0.5
                        && (*iter).vectStarMeasures[i].pairfPos.second > m_pGParam->m_SOpticData.fOptCenterY - szCropHeight * 0.5
                        && (*iter).vectStarMeasures[i].pairfPos.second < m_pGParam->m_SOpticData.fOptCenterY + szCropHeight * 0.5)
                {
                    measuresStar.vectStarMeasures.push_back((*iter).vectStarMeasures[i]);
                }
            }

            pair<float, float> pairfSpd, pairfSpd_AE;
            m_iNumMost = 0;
            m_pTrakcer->CalcStarSpd(measuresStar, pairfSpd, pairfSpd_AE, m_iNumMost);

            pairfSpd.first /= m_iBinning;
            pairfSpd.second /= m_iBinning;
            qDebug() << "### Star speed: (" << pairfSpd.first << "," << pairfSpd.second << ");  NumMost:" << m_iNumMost << ";  CropWidth:" << szCropWidth << ";  CropHeight: " << szCropHeight ;
            qDebug() << "### Full blobs:" << m_blobs.size() << "; Crop blobs:" << measuresStar.vectStarMeasures.size();
            m_pGParam->m_SImageProcessorData.pairfStarSpd = pairfSpd;
            m_pGParam->m_SImageProcessorData.uiNumStarMatch = m_iNumMost;
        }
        else
        {
            qDebug() << "### Full blobs:" << m_blobs.size();
        }
        iTime10 = ClockOff();
        qDebug() << "### Proc CalcStarSpd:" << iTime10 << "ms";

        /// 目标检测跟踪处理
        ClockOn();
        if(m_iTrackMode == TRACK_SC || m_iTrackMode == TRACK_LEO || m_iTrackMode == TRACK_MANUAL)
        {
            if(m_vectTargetInfo.empty())
                m_vectTargetInfo.push_back(sTargetInfo());
        }
        if (m_iTrackMode == TRACK_GEO)
        {
//            if (m_iNumMost >= 5 || (*iter).fExpTime <= 500.0)    // calculate star speed success or LEO Tracking
            {
                m_pTrakcer->TrackProc_GEO(measuresTarget, m_vectTargetInfo, m_pGParam->m_SImageProcessorData.bFullLEO, vecGEOisbValidBlob);
            }
        }
        else if (m_iTrackMode == TRACK_SC)
        {
            m_pTrakcer->TrackProc_SC(measuresTarget, m_vectTargetInfo[0]);
        }
        else if (m_iTrackMode == TRACK_LEO)
        {
            m_pTrakcer->TrackProc_LEO(measuresTarget, m_vectTargetInfo[0]);
        }
        else if (m_iTrackMode == TRACK_MANUAL)
        {
            if (m_pGParam->m_SImageProcessorData.bDoubleClicked)
            {
                m_pTrakcer->TrackProc_MANUAL(measuresTarget, m_vectTargetInfo[0], ManualBlob);
            }
        }
        if (m_iTrackMode == TRACK_GEO)
        {
            if (m_vectTargetInfo.size() > 25)
            {
                m_fRatioStd = 5.0;
            }
        }
        m_pGParam->m_SImageProcessorData.uiNumMeasure = measuresTarget.vectTargetMeasures.size();
        m_pGParam->m_SImageProcessorData.uiProcTrackMs = ClockOff();
        iTime11 = m_pGParam->m_SImageProcessorData.uiProcTrackMs;
        qDebug() << "### Proc Track:" << iTime11 << "ms";

        /// TWDW+GDCL
        ClockOn();
        if(m_iTrackMode != TRACK_SC && m_iTrackMode != TRACK_LEO)
        {
            unsigned int uiObsID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_pGParam->m_SObsParams.iObsID : m_pGParam->m_SImageReplayerData.qstrTeleID.toUInt();

            /// 目标刚刚完成前4帧航迹关联（当前搜索或跟踪流程第一次开始ProcessZXDW）
            bool bAssoc4 = true;
            for (int i = 0; i < m_vectTargetInfo.size(); i++)
            {
                bAssoc4 = bAssoc4 & (m_vectTargetInfo[i].vectInfoInFrame.size() == 4);
            }
            if (bAssoc4)
            {
                float fDist, fDistTemp;
                fDist = 1000000000;
                int iMainTargetSeq = 0;
                for (int i = 0; i < m_vectTargetInfo.size(); i++)
                {
                    fDistTemp = pow(m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX, 2.0)
                            + pow(m_pGParam->m_SOpticData.fOptCenterY - m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second, 2.0);
                    if (fDistTemp < fDist)
                    {
                        fDist = fDistTemp;
                        iMainTargetSeq = i;
                    }
                }
                if (fDist < 1000000000)
                {
                    m_iMainTargetSeq = iMainTargetSeq;
                }
            }

            vector<int> vecAllTargetInCurFrame, vecNewTargetInCurFrame;
            for(int i = 0; i < m_vectTargetInfo.size(); i++)
            {
                if(m_vectTargetInfo[i].bLiving)
                {
                    if(m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size() - 1].ulFrameSeq == (*iter).ulFrameSeq)
                    {
                        vecAllTargetInCurFrame.push_back(i);
                        if(m_vectTargetInfo[i].vectInfoInFrame.size() == 4)
                            vecNewTargetInCurFrame.push_back(i);
                    }
                }
            }
            if(vecNewTargetInCurFrame.size())
            {
                for(int iNew = 0; iNew < vecNewTargetInCurFrame.size(); iNew++)
                {
                    int i = vecNewTargetInCurFrame[iNew];
                    QString qstrTargetID;
                    qstrTargetID.sprintf("9%02d%03d", m_pGParam->m_SObsParams.iObsID % 100, i + 1);
                    if (i == m_iMainTargetSeq && m_SNetMasterControlData.qstrTargetID != "000000")
                    {
                        qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode
                                ? m_SNetMasterControlData.qstrTargetID
                                : m_pGParam->m_SImageReplayerData.qstrTargetID;
                    }
                    m_vectTargetInfo[i].qstrTargetID = qstrTargetID;
                    ///******************第1帧 ******************///
                    sMeasureBlob blob1 = m_vectTargetInfo[i].vectInfoInFrame[0].blobMeasure;
                    sResPacket resPacket1;
                    resPacket1.pairfFOVCenterAE = (*(iter - 3)).pairfFOVCenterAE;
                    resPacket1.pairfFOVCenterAEModify = pair<float, float>(blob1.fFOVCenterAziModify, blob1.fFOVCenterEleModify);
                    resPacket1.pairfTargetPosInFrame = blob1.pairfPos;
                    resPacket1.pairfTargetDistAE = pair<float, float>(blob1.fDistAzi, blob1.fDistEle);
                    resPacket1.pairfTargetPosZXDW = pair<float, float>(blob1.fTargetAzi, blob1.fTargetEle);
                    resPacket1.blob = blob1;
                    resPacket1.fExpTime = (*(iter - 3)).fExpTime;
                    resPacket1.fFrameFreq = (*(iter - 3)).fFrameFreq;
                    resPacket1.stimeFrame = (*(iter - 3)).stimeFrame;
                    resPacket1.ulFrameSeq = (*(iter - 3)).ulFrameSeq;
                    resPacket1.qstrTargetID = qstrTargetID;
                    resPacket1.fTemp = (*(iter - 3)).fTemp;
                    resPacket1.fAtmosP = (*(iter - 3)).fAtmosP;
                    resPacket1.fHumidity = (*(iter - 3)).fHumidity;
                    resPacket1.bValid = m_vectTargetInfo[i].vectInfoInFrame[0].bValid;
                    /// Calculate Atmos Error
                    double dAtmosErrEle1 = RefractVisual(((*(iter - 3)).pairfFOVCenterAE.second - blob1.dPointErrEle)/180.0*3.1415926,
                                                                               (*(iter - 3)).fAtmosP/100.0, (*(iter - 3)).fTemp) / 3.1415926 * 180.0;
                    ///******************第2帧 ******************///
                    sMeasureBlob blob2 = m_vectTargetInfo[i].vectInfoInFrame[1].blobMeasure;
                    sResPacket resPacket2;
                    resPacket2.pairfFOVCenterAE = (*(iter - 2)).pairfFOVCenterAE;
                    resPacket2.pairfFOVCenterAEModify = pair<float, float>(blob2.fFOVCenterAziModify, blob2.fFOVCenterEleModify);
                    resPacket2.pairfTargetPosInFrame = blob2.pairfPos;
                    resPacket2.pairfTargetDistAE = pair<float, float>(blob2.fDistAzi, blob2.fDistEle);
                    resPacket2.pairfTargetPosZXDW = pair<float, float>(blob2.fTargetAzi, blob2.fTargetEle);
                    resPacket2.blob = blob2;
                    resPacket2.fExpTime = (*(iter - 2)).fExpTime;
                    resPacket2.fFrameFreq = (*(iter - 2)).fFrameFreq;
                    resPacket2.stimeFrame = (*(iter - 2)).stimeFrame;
                    resPacket2.ulFrameSeq = (*(iter - 2)).ulFrameSeq;
                    resPacket2.qstrTargetID = qstrTargetID;
                    resPacket2.fTemp = (*(iter - 2)).fTemp;
                    resPacket2.fAtmosP = (*(iter - 2)).fAtmosP;
                    resPacket2.fHumidity = (*(iter - 2)).fHumidity;
                    resPacket2.bValid = m_vectTargetInfo[i].vectInfoInFrame[1].bValid;
                    /// Calculate Atmos Error
                    double dAtmosErrEle2 = RefractVisual(((*(iter - 2)).pairfFOVCenterAE.second - blob2.dPointErrEle)/180.0*3.1415926,
                                                                               (*(iter - 2)).fAtmosP/100.0, (*(iter - 2)).fTemp) / 3.1415926 * 180.0;
                    ///******************第3帧 ******************///
                    sMeasureBlob blob3 = m_vectTargetInfo[i].vectInfoInFrame[2].blobMeasure;
                    sResPacket resPacket3;
                    resPacket3.pairfFOVCenterAE = (*(iter - 1)).pairfFOVCenterAE;
                    resPacket3.pairfFOVCenterAEModify = pair<float, float>(blob3.fFOVCenterAziModify, blob3.fFOVCenterEleModify);
                    resPacket3.pairfTargetPosInFrame = blob3.pairfPos;
                    resPacket3.pairfTargetDistAE = pair<float, float>(blob3.fDistAzi, blob3.fDistEle);
                    resPacket3.pairfTargetPosZXDW = pair<float, float>(blob3.fTargetAzi, blob3.fTargetEle);
                    resPacket3.blob = blob3;
                    resPacket3.fExpTime = (*(iter - 1)).fExpTime;
                    resPacket3.fFrameFreq = (*(iter - 1)).fFrameFreq;
                    resPacket3.stimeFrame = (*(iter - 1)).stimeFrame;
                    resPacket3.ulFrameSeq = (*(iter - 1)).ulFrameSeq;
                    resPacket3.qstrTargetID = qstrTargetID;
                    resPacket3.fTemp = (*(iter - 1)).fTemp;
                    resPacket3.fAtmosP = (*(iter - 1)).fAtmosP;
                    resPacket3.fHumidity = (*(iter - 1)).fHumidity;
                    resPacket3.bValid = m_vectTargetInfo[i].vectInfoInFrame[2].bValid;
                    /// Calculate Atmos Error
                    double dAtmosErrEle3 = RefractVisual(((*(iter - 1)).pairfFOVCenterAE.second - blob3.dPointErrEle)/180.0*3.1415926,
                                                                               (*(iter - 1)).fAtmosP/100.0, (*(iter - 1)).fTemp) / 3.1415926 * 180.0;

                    m_vectTargetInfo[i].vectResPacket.push_back(resPacket1);
                    m_vectTargetInfo[i].vectResPacket.push_back(resPacket2);
                    m_vectTargetInfo[i].vectResPacket.push_back(resPacket3);

                    m_vectTargetInfo[i].vectulFrameSeqTWDW.push_back(resPacket1.ulFrameSeq);
                    m_vectTargetInfo[i].vectulFrameSeqTWDW.push_back(resPacket2.ulFrameSeq);
                    m_vectTargetInfo[i].vectulFrameSeqTWDW.push_back(resPacket3.ulFrameSeq);

                    /// 存储路径
                    QString qstrYYYY, qstrYYYYMM, qstrYYYYMMDD, qstrDatePath, qstrSub, qstrFileNameGAE;
                    if (m_pGParam->m_SImageProcessorData.bProcessMode)
                    {
                        if (m_SNetMasterControlData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_SNetMasterControlData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_SNetMasterControlData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_SNetMasterControlData.qstrStartTime.mid(0, 8);
                            qstrDatePath = qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_SNetMasterControlData.qstrStartTime + "_" + m_SNetMasterControlData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
    //                        qstrFileNameGAE = m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + qstrTargetID + "_" + QString::number(uiObsID) + ".GAE";
                        }
                    }
                    else
                    {
                        if (m_pGParam->m_SImageReplayerData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 8);
                            qstrDatePath = "Replay/" + qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_pGParam->m_SImageReplayerData.qstrStartTime + "_" + m_pGParam->m_SImageReplayerData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
                        }
                    }
                    if (m_qstrStorePath != m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub)
                        m_qstrStorePath = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub;
                    QDir qDirMake;
                    if (!qDirMake.exists(m_qstrStorePath + "/GAE"))
                    {
                        qDirMake.mkpath(m_qstrStorePath + "/GAE");
                    }

                    QString qstrTaskID = qstrTargetID;
                    QString qstrEndTime;
                    qstrEndTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                        resPacket3.stimeFrame.iYear,
                                        resPacket3.stimeFrame.iMonth,
                                        resPacket3.stimeFrame.iDay,
                                        resPacket3.stimeFrame.iHour,
                                        resPacket3.stimeFrame.iMinute,
                                        resPacket3.stimeFrame.iSecond);
                    /// 数据整形
                    char strDWData[300]="";
                    char strDWDataNew[300]="";
                    char strExpTime[300]="";
                    int nYear, nMonth, nDay, nHour, nMinute, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
                    double dblSecond, dblSecondNew, dSecondAdd, dblHourNew, dblPointA, dblPointE, dblwendu, dbldqy, dblshidu;
                    ///******************resPacket1 ******************///
                    BJ2UTC(resPacket1.stimeFrame.iYear,
                            resPacket1.stimeFrame.iMonth,
                            resPacket1.stimeFrame.iDay,
                            resPacket1.stimeFrame.iHour,
                            nYear, nMonth, nDay, nHour);
                    nMinute = resPacket1.stimeFrame.iMinute;
                    dblSecond = resPacket1.stimeFrame.iSecond +resPacket1.stimeFrame.iMillisecond * 0.001 + resPacket1.stimeFrame.iMicrosecond * 0.000001;
                    dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - resPacket1.blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
                    TimeAddSeconds(nYear, nMonth, nDay, nHour, nMinute, dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
                    dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
                    dblPointA = resPacket1.pairfTargetPosZXDW.first;
                    dblPointE = resPacket1.pairfTargetPosZXDW.second - dAtmosErrEle1;
                    dblwendu = resPacket1.fTemp+273.15;
                    dbldqy = resPacket1.fAtmosP/100.0;
                    dblshidu = resPacket1.fHumidity;
                    m_pstarmap->ZXDWDataNewFormat(qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,
                                                  dblPointA,dblPointE,
                                                  dblwendu-273.15,dblshidu,dbldqy, true,0,strDWData);
                    ChangeGAEStr(strDWData, strDWDataNew, strExpTime);
                    GetFirstResPackTime(strDWDataNew, m_vectTargetInfo[i].qstrSaveStartTime);
                    m_vectTargetInfo[i].qstrFileNameGAE = m_vectTargetInfo[i].qstrSaveStartTime + "_" + qstrTargetID + "_" + QString::number(uiObsID) + ".GAE";
                    {
                        sPacketGAE packet;
                        packet.qstrStorePath = m_qstrStorePath + "/GAE";
                        packet.qstrFileNameGAE = m_vectTargetInfo[i].qstrFileNameGAE;
                        packet.qstrTaskID = qstrTaskID;
                        packet.qstrObsID = QString::number(uiObsID);
                        packet.qstrStartTime = m_vectTargetInfo[i].qstrSaveStartTime;
                        packet.qstrEndTime = qstrEndTime;
                        memcpy(packet.strExpTime, strExpTime, sizeof(strExpTime));
                        memcpy(packet.strDWDataNew, strDWDataNew, sizeof(strDWDataNew));
                        if(resPacket1.bValid)
                            m_vectpacketGAE.push_back(packet);
                    }
                    ///******************resPacket2 ******************///
                    BJ2UTC(resPacket2.stimeFrame.iYear,
                            resPacket2.stimeFrame.iMonth,
                            resPacket2.stimeFrame.iDay,
                            resPacket2.stimeFrame.iHour,
                            nYear, nMonth, nDay, nHour);
                    nMinute = resPacket2.stimeFrame.iMinute;
                    dblSecond = resPacket2.stimeFrame.iSecond +resPacket2.stimeFrame.iMillisecond * 0.001 + resPacket2.stimeFrame.iMicrosecond * 0.000001;
                    dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - resPacket2.blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
                    TimeAddSeconds(nYear, nMonth, nDay, nHour, nMinute, dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
                    dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
                    dblPointA = resPacket2.pairfTargetPosZXDW.first;
                    dblPointE = resPacket2.pairfTargetPosZXDW.second - dAtmosErrEle2;
                    dblwendu = resPacket2.fTemp+273.15;
                    dbldqy = resPacket2.fAtmosP/100.0;
                    dblshidu = resPacket2.fHumidity;
                    m_pstarmap->ZXDWDataNewFormat(qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,
                                                  dblPointA,dblPointE,
                                                  dblwendu-273.15,dblshidu,dbldqy, true,0,strDWData);
                    ChangeGAEStr(strDWData, strDWDataNew, strExpTime);
                    {
                        sPacketGAE packet;
                        packet.qstrStorePath = m_qstrStorePath + "/GAE";
                        packet.qstrFileNameGAE = m_vectTargetInfo[i].qstrFileNameGAE;
                        packet.qstrTaskID = qstrTaskID;
                        packet.qstrObsID = QString::number(uiObsID);
                        packet.qstrStartTime = m_vectTargetInfo[i].qstrSaveStartTime;
                        packet.qstrEndTime = qstrEndTime;
                        memcpy(packet.strExpTime, strExpTime, sizeof(strExpTime));
                        memcpy(packet.strDWDataNew, strDWDataNew, sizeof(strDWDataNew));
                        if(resPacket2.bValid)
                            m_vectpacketGAE.push_back(packet);
                    }
                    /// resPacket3
                    BJ2UTC(resPacket3.stimeFrame.iYear,
                            resPacket3.stimeFrame.iMonth,
                            resPacket3.stimeFrame.iDay,
                            resPacket3.stimeFrame.iHour,
                            nYear, nMonth, nDay, nHour);
                    nMinute = resPacket3.stimeFrame.iMinute;
                    dblSecond = resPacket3.stimeFrame.iSecond +resPacket3.stimeFrame.iMillisecond * 0.001 + resPacket3.stimeFrame.iMicrosecond * 0.000001;
                    dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - resPacket3.blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
                    TimeAddSeconds(nYear, nMonth, nDay, nHour, nMinute, dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
                    dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
                    dblPointA = resPacket3.pairfTargetPosZXDW.first;
                    dblPointE = resPacket3.pairfTargetPosZXDW.second - dAtmosErrEle3;
                    dblwendu = resPacket3.fTemp+273.15;
                    dbldqy = resPacket3.fAtmosP/100.0;
                    dblshidu = resPacket3.fHumidity;
                    m_pstarmap->ZXDWDataNewFormat(qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,
                                                  dblPointA,dblPointE,
                                                  dblwendu-273.15,dblshidu,dbldqy, true,0,strDWData);
                    ChangeGAEStr(strDWData, strDWDataNew, strExpTime);
                    {
                        sPacketGAE packet;
                        packet.qstrStorePath = m_qstrStorePath + "/GAE";
                        packet.qstrFileNameGAE = m_vectTargetInfo[i].qstrFileNameGAE;
                        packet.qstrTaskID = qstrTaskID;
                        packet.qstrObsID = QString::number(uiObsID);
                        packet.qstrStartTime = m_vectTargetInfo[i].qstrSaveStartTime;
                        packet.qstrEndTime = qstrEndTime;
                        memcpy(packet.strExpTime, strExpTime, sizeof(strExpTime));
                        memcpy(packet.strDWDataNew, strDWDataNew, sizeof(strDWDataNew));
                        if(resPacket3.bValid)
                            m_vectpacketGAE.push_back(packet);
                    }
                    //*******************************************************// Vedio标注1
                    {
                        QDir qDirMake;
                        if (!qDirMake.exists(m_qstrStorePath + "/TAG"))
                        {
                            qDirMake.mkpath(m_qstrStorePath + "/TAG");
                        }
                        QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_SNetMasterControlData.qstrTargetID
                                                           : m_pGParam->m_SImageReplayerData.qstrTargetID;
                        QString qstrTeleID = m_pGParam->m_SImageProcessorData.bProcessMode ? QString::number(m_pGParam->m_SObsParams.iObsID)
                                                         : m_pGParam->m_SImageReplayerData.qstrTeleID;
                        QString qstrJSTagFileName;
                        qstrJSTagFileName.sprintf("%04d%02d%02d%02d%02d%02d%03d_%06d_%04d_%06d.tag",
                                                  (*(iter - 3)).stimeFrame.iYear,
                                                  (*(iter - 3)).stimeFrame.iMonth,
                                                  (*(iter - 3)).stimeFrame.iDay,
                                                  (*(iter - 3)).stimeFrame.iHour,
                                                  (*(iter - 3)).stimeFrame.iMinute,
                                                  (*(iter - 3)).stimeFrame.iSecond,
                                                  (*(iter - 3)).stimeFrame.iMillisecond,
                                                  qstrTargetID.toUInt(),
                                                  qstrTeleID.toUInt(),
                                                  (unsigned int)(*(iter - 3)).ulFrameSeq);
                        WriteVedioTagFile(m_qstrStorePath + "/TAG", qstrJSTagFileName, resPacket1.blob, resPacket1.qstrTargetID);
                    }
                    //*******************************************************// Vedio标注2
                    {
                        QDir qDirMake;
                        if (!qDirMake.exists(m_qstrStorePath + "/TAG"))
                        {
                            qDirMake.mkpath(m_qstrStorePath + "/TAG");
                        }
                        QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_SNetMasterControlData.qstrTargetID
                                                           : m_pGParam->m_SImageReplayerData.qstrTargetID;
                        QString qstrTeleID = m_pGParam->m_SImageProcessorData.bProcessMode ? QString::number(m_pGParam->m_SObsParams.iObsID)
                                                         : m_pGParam->m_SImageReplayerData.qstrTeleID;
                        QString qstrJSTagFileName;
                        qstrJSTagFileName.sprintf("%04d%02d%02d%02d%02d%02d%03d_%06d_%04d_%06d.tag",
                                                  (*(iter - 2)).stimeFrame.iYear,
                                                  (*(iter - 2)).stimeFrame.iMonth,
                                                  (*(iter - 2)).stimeFrame.iDay,
                                                  (*(iter - 2)).stimeFrame.iHour,
                                                  (*(iter - 2)).stimeFrame.iMinute,
                                                  (*(iter - 2)).stimeFrame.iSecond,
                                                  (*(iter - 2)).stimeFrame.iMillisecond,
                                                  qstrTargetID.toUInt(),
                                                  qstrTeleID.toUInt(),
                                                  (unsigned int)(*(iter - 2)).ulFrameSeq);
                        WriteVedioTagFile(m_qstrStorePath + "/TAG", qstrJSTagFileName, resPacket2.blob, resPacket2.qstrTargetID);
                    }
                    //*******************************************************// Vedio标注3
                    {
                        QDir qDirMake;
                        if (!qDirMake.exists(m_qstrStorePath + "/TAG"))
                        {
                            qDirMake.mkpath(m_qstrStorePath + "/TAG");
                        }
                        QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_SNetMasterControlData.qstrTargetID
                                                           : m_pGParam->m_SImageReplayerData.qstrTargetID;
                        QString qstrTeleID = m_pGParam->m_SImageProcessorData.bProcessMode ? QString::number(m_pGParam->m_SObsParams.iObsID)
                                                         : m_pGParam->m_SImageReplayerData.qstrTeleID;
                        QString qstrJSTagFileName;
                        qstrJSTagFileName.sprintf("%04d%02d%02d%02d%02d%02d%03d_%06d_%04d_%06d.tag",
                                                  (*(iter - 1)).stimeFrame.iYear,
                                                  (*(iter - 1)).stimeFrame.iMonth,
                                                  (*(iter - 1)).stimeFrame.iDay,
                                                  (*(iter - 1)).stimeFrame.iHour,
                                                  (*(iter - 1)).stimeFrame.iMinute,
                                                  (*(iter - 1)).stimeFrame.iSecond,
                                                  (*(iter - 1)).stimeFrame.iMillisecond,
                                                  qstrTargetID.toUInt(),
                                                  qstrTeleID.toUInt(),
                                                  (unsigned int)(*(iter - 1)).ulFrameSeq);
                        WriteVedioTagFile(m_qstrStorePath + "/TAG", qstrJSTagFileName, resPacket3.blob, resPacket3.qstrTargetID);
                    }
                }
                ProcessTWDW_GDCL(vecNewTargetInCurFrame, (*iter).ulFrameSeq - 3);
                ProcessTWDW_GDCL(vecNewTargetInCurFrame, (*iter).ulFrameSeq - 2);
                ProcessTWDW_GDCL(vecNewTargetInCurFrame, (*iter).ulFrameSeq - 1);
            }

            sGXTCHeader header;
            header.iNumTargets = 0;
            vector<sGXTCData> vectGXTCData;
            if(vecAllTargetInCurFrame.size())
            {
                for(int iAll = 0; iAll < vecAllTargetInCurFrame.size(); iAll++)
                {
                    int i = vecAllTargetInCurFrame[iAll];
                    ///******************当前帧******************///
                    /// Result Packet
                    sMeasureBlob blob = m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure;
                    sResPacket resPacket;
                    resPacket.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
                    resPacket.pairfFOVCenterAEModify = pair<float, float>(blob.fFOVCenterAziModify, blob.fFOVCenterEleModify);
                    resPacket.pairfTargetPosInFrame = blob.pairfPos;
                    resPacket.pairfTargetDistAE = pair<float, float>(blob.fDistAzi, blob.fDistEle);
                    resPacket.pairfTargetPosZXDW = pair<float, float>(blob.fTargetAzi, blob.fTargetEle);
                    resPacket.blob = blob;
                    resPacket.fExpTime = (*iter).fExpTime;
                    resPacket.fFrameFreq = (*iter).fFrameFreq;
                    resPacket.stimeFrame = (*iter).stimeFrame;
                    resPacket.ulFrameSeq = (*iter).ulFrameSeq;
                    resPacket.qstrTargetID = m_vectTargetInfo[i].qstrTargetID;
                    resPacket.fTemp = (*iter).fTemp;
                    resPacket.fAtmosP = (*iter).fAtmosP;
                    resPacket.fHumidity = (*iter).fHumidity;
                    resPacket.bValid = m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].bValid;
                    /// Calculate Atmos Error
                    double dAtmosErrEle = RefractVisual(((*iter).pairfFOVCenterAE.second - blob.dPointErrEle)/180.0*3.1415926,
                                                                               (*iter).fAtmosP/100.0, (*iter).fTemp) / 3.1415926 * 180.0;

                    m_vectTargetInfo[i].vectResPacket.push_back(resPacket);
                    if (m_vectTargetInfo[i].vectulFrameSeqTWDW.size() > 100)
                        m_vectTargetInfo[i].vectulFrameSeqTWDW.clear();
                    m_vectTargetInfo[i].vectulFrameSeqTWDW.push_back(resPacket.ulFrameSeq);

                    /// 存储路径
                    QString qstrYYYY, qstrYYYYMM, qstrYYYYMMDD, qstrDatePath, qstrSub, qstrFileNameGAE;
                    if (m_pGParam->m_SImageProcessorData.bProcessMode)
                    {
                        if (m_SNetMasterControlData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_SNetMasterControlData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_SNetMasterControlData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_SNetMasterControlData.qstrStartTime.mid(0, 8);
                            qstrDatePath = qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_SNetMasterControlData.qstrStartTime + "_" + m_SNetMasterControlData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
    //                        qstrFileNameGAE = m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + m_vectTargetInfo[i].qstrTargetID + "_" + QString::number(uiObsID) + ".GAE";
                        }
                    }
                    else
                    {
                        if (m_pGParam->m_SImageReplayerData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 8);
                            qstrDatePath = "Replay/" + qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_pGParam->m_SImageReplayerData.qstrStartTime + "_" + m_pGParam->m_SImageReplayerData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
                        }
                    }
                    if (m_qstrStorePath != m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub)
                    {
                        m_qstrStorePath = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub;
                    }
                    QDir qDirMake;
                    if (!qDirMake.exists(m_qstrStorePath + "/GAE"))
                    {
                        qDirMake.mkpath(m_qstrStorePath + "/GAE");
                    }

                    /// 数据整形
                    char strDWData[300]="";
                    char strDWDataNew[300]="";
                    char strExpTime[300]="";
                    int nYear, nMonth, nDay, nHour, nMinute, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
                    double dblSecond, dblSecondNew, dSecondAdd, dblHourNew, dblPointA, dblPointE, dblwendu, dbldqy, dblshidu;
                    BJ2UTC(resPacket.stimeFrame.iYear,
                            resPacket.stimeFrame.iMonth,
                            resPacket.stimeFrame.iDay,
                            resPacket.stimeFrame.iHour,
                            nYear, nMonth, nDay, nHour);
                    nMinute = resPacket.stimeFrame.iMinute;
                    dblSecond = resPacket.stimeFrame.iSecond +resPacket.stimeFrame.iMillisecond * 0.001 + resPacket.stimeFrame.iMicrosecond * 0.000001;
                    dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - resPacket.blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
                    TimeAddSeconds(nYear, nMonth, nDay, nHour, nMinute, dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
                    dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
                    dblPointA = resPacket.pairfTargetPosZXDW.first;
                    dblPointE = resPacket.pairfTargetPosZXDW.second - dAtmosErrEle;
                    dblwendu = resPacket.fTemp+273.15;
                    dbldqy = resPacket.fAtmosP/100.0;
                    dblshidu = resPacket.fHumidity;
                    m_pstarmap->ZXDWDataNewFormat(m_vectTargetInfo[i].qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,
                                                  dblPointA,dblPointE,
                                                  dblwendu-273.15,dblshidu,dbldqy, true,0,strDWData);
                    ChangeGAEStr(strDWData, strDWDataNew, strExpTime);
                    QString qstrEndTime;
                    qstrEndTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                        resPacket.stimeFrame.iYear,
                                        resPacket.stimeFrame.iMonth,
                                        resPacket.stimeFrame.iDay,
                                        resPacket.stimeFrame.iHour,
                                        resPacket.stimeFrame.iMinute,
                                        resPacket.stimeFrame.iSecond);
                    {
                        sPacketGAE packet;
                        packet.qstrStorePath = m_qstrStorePath + "/GAE";
                        packet.qstrFileNameGAE = m_vectTargetInfo[i].qstrFileNameGAE;
                        packet.qstrTaskID = m_vectTargetInfo[i].qstrTargetID;
                        packet.qstrObsID = QString::number(uiObsID);
                        packet.qstrStartTime = m_vectTargetInfo[i].qstrSaveStartTime;
                        packet.qstrEndTime = qstrEndTime;
                        memcpy(packet.strExpTime, strExpTime, sizeof(strExpTime));
                        memcpy(packet.strDWDataNew, strDWDataNew, sizeof(strDWDataNew));
                        if(resPacket.bValid)
                            m_vectpacketGAE.push_back(packet);                       
                    }

                    //*******************************************************// Vedio标注
                    {
                        QDir qDirMake;
                        if (!qDirMake.exists(m_qstrStorePath + "/TAG"))
                        {
                            qDirMake.mkpath(m_qstrStorePath + "/TAG");
                        }
                        QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_SNetMasterControlData.qstrTargetID
                                                           : m_pGParam->m_SImageReplayerData.qstrTargetID;
                        QString qstrTeleID = m_pGParam->m_SImageProcessorData.bProcessMode ? QString::number(m_pGParam->m_SObsParams.iObsID)
                                                         : m_pGParam->m_SImageReplayerData.qstrTeleID;
                        QString qstrJSTagFileName;
                        qstrJSTagFileName.sprintf("%04d%02d%02d%02d%02d%02d%03d_%06d_%04d_%06d.tag",
                                                  (*iter).stimeFrame.iYear,
                                                  (*iter).stimeFrame.iMonth,
                                                  (*iter).stimeFrame.iDay,
                                                  (*iter).stimeFrame.iHour,
                                                  (*iter).stimeFrame.iMinute,
                                                  (*iter).stimeFrame.iSecond,
                                                  (*iter).stimeFrame.iMillisecond,
                                                  qstrTargetID.toUInt(),
                                                  qstrTeleID.toUInt(),
                                                  (unsigned int)(*iter).ulFrameSeq);
                        WriteVedioTagFile(m_qstrStorePath + "/TAG", qstrJSTagFileName, resPacket.blob, resPacket.qstrTargetID);
                    }
                    //*******************************************************//
                    header.iNumTargets++;
                    QDate dateNow(nYearNew, nMonthNew, nDayNew);
                    int iDaysNow = dateNow.toJulianDay();
                    QDate date1970(1970, 1, 1);
                    int iDays1970 = date1970.toJulianDay();
                    int iJD1970 = iDaysNow - iDays1970;
                    header.lJMS1970 = (long)iJD1970 * 24 * 3600 * 100 + (long)(dblHourNew * 3600.0) * 100;
                    header.cMeasureStatus = m_SNetMasterControlData.bSearch ? 1 : 2;
                    header.cTaskMode = 2;
                    header.cTargetStatus = resPacket.bValid ? 1 : 2;
                    header.dTemp = dblwendu-273.15;
                    header.dAtmosP = dbldqy*100.0;
                    header.dHumidity = dblshidu*100.0;

                    sGXTCData data;
                    data.cMainFlag = 0xFF;
                    data.iTargetID = m_vectTargetInfo[i].qstrTargetID.toUInt();
                    data.iAzi = (int)(dblPointA * 3600.0);
                    data.iEle = (int)(dblPointE * 3600.0);
                    data.iAziSpd = (int)(m_vectTargetInfo[i].pairfPredSpdAE.first * 3600.0);
                    data.iEleSpd = (int)(m_vectTargetInfo[i].pairfPredSpdAE.second * 3600.0);
                    data.dMv = 0;
                    vectGXTCData.push_back(data);
                }
                if (m_pGParam->m_SImageProcessorData.bProcessMode)
                {
                    char* pacSendData = new char[sizeof(sGXTCHeader)+sizeof(sGXTCData)*vectGXTCData.size()];
                    memcpy(pacSendData, (char*)&header, sizeof(sGXTCHeader));
                    for (int i = 0; i < vectGXTCData.size(); i++)
                    {
                        memcpy(pacSendData + sizeof(sGXTCHeader) + sizeof(sGXTCData) * i, (char*)&vectGXTCData[i], sizeof(sGXTCData));
                    }
                    NetExchange* pNet = (NetExchange*)m_pGParam->m_SNetExchangeData.pvoidThis;
                    pNet->SendGXTC(pacSendData, sizeof(sGXTCHeader)+sizeof(sGXTCData)*vectGXTCData.size());
                }
                ProcessTWDW_GDCL(vecAllTargetInCurFrame, (*iter).ulFrameSeq);
            }
        }
        else
        {
            unsigned int uiObsID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_pGParam->m_SObsParams.iObsID : m_pGParam->m_SImageReplayerData.qstrTeleID.toUInt();

            /// 刚刚完成前3帧航迹关联
            int i = 0;
            QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode
                    ? m_SNetMasterControlData.qstrTargetID
                    : m_pGParam->m_SImageReplayerData.qstrTargetID;;
            m_vectTargetInfo[i].qstrTargetID = qstrTargetID;
            if (m_vectTargetInfo[0].vectInfoInFrame.size() >= 3)
            {
                ///******************当前帧******************///
                /// Result Packet
                sMeasureBlob blob = m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure;
                sResPacket resPacket;
                resPacket.pairfFOVCenterAE = (*iter).pairfFOVCenterAE;
                resPacket.pairfFOVCenterAEModify = pair<float, float>(blob.fFOVCenterAziModify, blob.fFOVCenterEleModify);
                resPacket.pairfTargetPosInFrame = blob.pairfPos;
                resPacket.pairfTargetDistAE = pair<float, float>(blob.fDistAzi, blob.fDistEle);
                resPacket.pairfTargetPosZXDW = pair<float, float>(blob.fTargetAzi, blob.fTargetEle);
                resPacket.blob = blob;
                resPacket.fExpTime = (*iter).fExpTime;
                resPacket.fFrameFreq = (*iter).fFrameFreq;
                resPacket.stimeFrame = (*iter).stimeFrame;
                resPacket.ulFrameSeq = (*iter).ulFrameSeq;
                resPacket.qstrTargetID = m_vectTargetInfo[i].qstrTargetID;
                resPacket.fTemp = (*iter).fTemp;
                resPacket.fAtmosP = (*iter).fAtmosP;
                resPacket.fHumidity = (*iter).fHumidity;
                resPacket.bValid = m_vectTargetInfo[i].vectInfoInFrame[m_vectTargetInfo[i].vectInfoInFrame.size()-1].bValid;
                /// Calculate Atmos Error
                double dAtmosErrEle = RefractVisual(((*iter).pairfFOVCenterAE.second - blob.dPointErrEle)/180.0*3.1415926,
                                                                           (*iter).fAtmosP/100.0, (*iter).fTemp) / 3.1415926 * 180.0;

                m_vectTargetInfo[i].vectResPacket.push_back(resPacket);
                if (m_iTrackMode == TRACK_LEO)
                {
                    /// 存储路径
                    QString qstrYYYY, qstrYYYYMM, qstrYYYYMMDD, qstrDatePath, qstrSub, qstrFileNameGAE;
                    if (m_pGParam->m_SImageProcessorData.bProcessMode)
                    {
                        if (m_SNetMasterControlData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_SNetMasterControlData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_SNetMasterControlData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_SNetMasterControlData.qstrStartTime.mid(0, 8);
                            qstrDatePath = qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_SNetMasterControlData.qstrStartTime + "_" + m_SNetMasterControlData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
    //                        qstrFileNameGAE = m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + m_vectTargetInfo[i].qstrTargetID + "_" + QString::number(uiObsID) + ".GAE";
                        }
                    }
                    else
                    {
                        if (m_pGParam->m_SImageReplayerData.qstrStartTime.size() >= 14)
                        {
                            qstrYYYY = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 4);
                            qstrYYYYMM = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 6);
                            qstrYYYYMMDD = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 8);
                            qstrDatePath = "Replay/" + qstrYYYY + "/"
                                    + qstrYYYYMM + "/"
                                    + qstrYYYYMMDD + "/";
                            qstrSub = qstrDatePath + m_pGParam->m_SImageReplayerData.qstrStartTime + "_" + m_pGParam->m_SImageReplayerData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
                        }
                    }
                    if (m_qstrStorePath != m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub)
                    {
                        m_qstrStorePath = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub;
                    }
                    QDir qDirMake;
                    if (!qDirMake.exists(m_qstrStorePath + "/GAE"))
                    {
                        qDirMake.mkpath(m_qstrStorePath + "/GAE");
                    }

                    /// 数据整形
                    char strDWData[300]="";
                    char strDWDataNew[300]="";
                    char strExpTime[300]="";
                    int nYear, nMonth, nDay, nHour, nMinute, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
                    double dblSecond, dblSecondNew, dSecondAdd, dblHourNew, dblPointA, dblPointE, dblwendu, dbldqy, dblshidu;
                    BJ2UTC(resPacket.stimeFrame.iYear,
                            resPacket.stimeFrame.iMonth,
                            resPacket.stimeFrame.iDay,
                            resPacket.stimeFrame.iHour,
                            nYear, nMonth, nDay, nHour);
                    nMinute = resPacket.stimeFrame.iMinute;
                    dblSecond = resPacket.stimeFrame.iSecond +resPacket.stimeFrame.iMillisecond * 0.001 + resPacket.stimeFrame.iMicrosecond * 0.000001;
                    dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - resPacket.blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
                    TimeAddSeconds(nYear, nMonth, nDay, nHour, nMinute, dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
                    dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
                    dblPointA = resPacket.pairfTargetPosZXDW.first;
                    dblPointE = resPacket.pairfTargetPosZXDW.second - dAtmosErrEle;
                    dblwendu = resPacket.fTemp+273.15;
                    dbldqy = resPacket.fAtmosP/100.0;
                    dblshidu = resPacket.fHumidity;
                    m_pstarmap->ZXDWDataNewFormat(m_vectTargetInfo[i].qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,
                                                  dblPointA,dblPointE,
                                                  dblwendu-273.15,dblshidu,dbldqy, true,0,strDWData);
                    ChangeGAEStr(strDWData, strDWDataNew, strExpTime);
                    if (m_vectTargetInfo[0].vectInfoInFrame.size() == 4)
                    {
                        GetFirstResPackTime(strDWDataNew, m_vectTargetInfo[i].qstrSaveStartTime);
                        m_vectTargetInfo[i].qstrFileNameGAE = m_vectTargetInfo[i].qstrSaveStartTime + "_" + qstrTargetID + "_" + QString::number(uiObsID) + ".GAE";
                    }
                    QString qstrEndTime;
                    qstrEndTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                        resPacket.stimeFrame.iYear,
                                        resPacket.stimeFrame.iMonth,
                                        resPacket.stimeFrame.iDay,
                                        resPacket.stimeFrame.iHour,
                                        resPacket.stimeFrame.iMinute,
                                        resPacket.stimeFrame.iSecond);
                    {
                        sPacketGAE packet;
                        packet.qstrStorePath = m_qstrStorePath + "/GAE";
                        packet.qstrFileNameGAE = m_vectTargetInfo[i].qstrFileNameGAE;
                        packet.qstrTaskID = m_vectTargetInfo[i].qstrTargetID;
                        packet.qstrObsID = QString::number(uiObsID);
                        packet.qstrStartTime = m_vectTargetInfo[i].qstrSaveStartTime;
                        packet.qstrEndTime = qstrEndTime;
                        memcpy(packet.strExpTime, strExpTime, sizeof(strExpTime));
                        memcpy(packet.strDWDataNew, strDWDataNew, sizeof(strDWDataNew));
                        if(resPacket.bValid)
                            m_vectpacketGAE.push_back(packet);
                        if(!m_vectpacketGAE.empty())
                        {
                            sPacketGAE packet_1 = m_vectpacketGAE[m_vectpacketGAE.size() - 1];
                            if (strcmp(strExpTime, packet_1.strExpTime) == 0
                                    && packet_1.qstrObsID == QString::number(m_pGParam->m_SObsParams.iObsID))
                            {
                                char strDWDataNew[300]="";
                                ChangeGAEStrMv(packet_1.strDWDataNew, strDWDataNew, 0.0);
                                WriteFile(packet_1.qstrStorePath,
                                          packet_1.qstrFileNameGAE,
                                          packet_1.qstrTaskID,
                                          packet_1.qstrObsID,
                                          packet_1.qstrStartTime,
                                          packet_1.qstrEndTime,
                                          strDWDataNew);
                                m_vectpacketGAE.erase(m_vectpacketGAE.end() - 1);
                            }
                        }
                    }

                    //*******************************************************// Vedio标注
                    {
                        QDir qDirMake;
                        if (!qDirMake.exists(m_qstrStorePath + "/TAG"))
                        {
                            qDirMake.mkpath(m_qstrStorePath + "/TAG");
                        }
                        QString qstrTargetID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_SNetMasterControlData.qstrTargetID
                                                           : m_pGParam->m_SImageReplayerData.qstrTargetID;
                        QString qstrTeleID = m_pGParam->m_SImageProcessorData.bProcessMode ? QString::number(m_pGParam->m_SObsParams.iObsID)
                                                         : m_pGParam->m_SImageReplayerData.qstrTeleID;
                        QString qstrJSTagFileName;
                        qstrJSTagFileName.sprintf("%04d%02d%02d%02d%02d%02d%03d_%06d_%04d_%06d.tag",
                                                  (*iter).stimeFrame.iYear,
                                                  (*iter).stimeFrame.iMonth,
                                                  (*iter).stimeFrame.iDay,
                                                  (*iter).stimeFrame.iHour,
                                                  (*iter).stimeFrame.iMinute,
                                                  (*iter).stimeFrame.iSecond,
                                                  (*iter).stimeFrame.iMillisecond,
                                                  qstrTargetID.toUInt(),
                                                  qstrTeleID.toUInt(),
                                                  (unsigned int)(*iter).ulFrameSeq);
                        WriteVedioTagFile(m_qstrStorePath + "/TAG", qstrJSTagFileName, resPacket.blob, resPacket.qstrTargetID);
                    }
                }
            }
        }
        m_pGParam->m_SDataProcessorData.iTWDWTime = ClockOff();
        iTime12 = m_pGParam->m_SDataProcessorData.iTWDWTime;
        qDebug() << "### Proc TWDW & GDCL:" << iTime12 << "ms";


        ClockOn();
        if (m_bDispBW)
        {
            clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmNoStarBW, m_cmDisp, 0, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), 0, NULL, NULL);
        }
        else
        {
            if (!m_bDispEnhance)	// 显示图像增强
            {
                if (m_bAutoScale)
                {
                    unsigned int uiScaleMin = (*iter).sausImageEnhance.dAvg - 3 * (*iter).sausImageEnhance.dStd < 0 ? 0 : (unsigned int)((*iter).sausImageEnhance.dAvg - 3 * (*iter).sausImageEnhance.dStd);
                    unsigned int uiScaleMax = (*iter).sausImageEnhance.dAvg + 3 * (*iter).sausImageEnhance.dStd > 16383 ? 16383 : (unsigned int)((*iter).sausImageEnhance.dAvg + 3 * (*iter).sausImageEnhance.dStd);
                    Short2ByteMinMaxGPU(m_cmEnhance, m_cmDisp, uiScaleMin, uiScaleMax, m_szImageWidth, m_szImageHeight);
                }
                else
                {
                    Short2ByteMinMaxGPU(m_cmEnhance, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
                }
                m_pGParam->m_SImageProcessorData.fAvr = (*iter).sausImageEnhance.dAvg;
                m_pGParam->m_SImageProcessorData.fStd = (*iter).sausImageEnhance.dStd;
            }
            else // 显示图像不增强
            {
                if (m_bAutoScale)
                {
                    unsigned int uiScaleMin = (*iter).sausImageGrab.dAvg - 3 * (*iter).sausImageGrab.dStd < 0 ? 0 : (unsigned int)((*iter).sausImageGrab.dAvg - 3 * (*iter).sausImageGrab.dStd);
                    unsigned int uiScaleMax = (*iter).sausImageGrab.dAvg + 3 * (*iter).sausImageGrab.dStd > 16383 ? 16383 : (unsigned int)((*iter).sausImageGrab.dAvg + 3 * (*iter).sausImageGrab.dStd);
                    Short2ByteMinMaxGPU(m_cmEnhancePre, m_cmDisp, uiScaleMin, uiScaleMax, m_szImageWidth, m_szImageHeight);
                }
                else
                {
                    Short2ByteMinMaxGPU(m_cmEnhancePre, m_cmDisp, m_uiScaleMin, m_uiScaleMax, m_szImageWidth, m_szImageHeight);
                }
                m_pGParam->m_SImageProcessorData.fAvr = (*iter).sausImageGrab.dAvg;
                m_pGParam->m_SImageProcessorData.fStd = (*iter).sausImageGrab.dStd;
            }
        }
        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmDisp, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned char), m_paucDisp, 0, NULL, NULL);
        iTime13 = ClockOff();
        qDebug() << "### Proc Disp:" << iTime13 << "ms";
        m_pGParam->m_SImageProcessorData.iProcTime += iTime1 + iTime2 + iTime3 + iTime4 + iTime5 + iTime6 + iTime7 + iTime8 + iTime9 + iTime10 + iTime11 + iTime12 + iTime13;
    }

    return 0;
}

void ImageProcAlgo::SetBlobAreaLimit(int iMinArea, int iMaxArea)
{
    if (iMaxArea > 0 && iMinArea > 0 && iMaxArea > iMinArea)
    {
        m_iMaxArea = iMaxArea;
        m_iMinArea = iMinArea;
    }
}

int ImageProcAlgo::AvgStdGPU16(cl_mem cmInput, double* pdAvg, double* pdStdev, size_t szImageWidth, size_t szImageHeight)
{
	double dAvg = 0;
	double dVar = 0;
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szWorkGroup = DivUp(szImageWidth, szLocalWorkSize);
	size_t szGlobalWorkSize = (szWorkGroup * szLocalWorkSize);
	size_t pixels = szImageWidth * szImageHeight;
	cl_mem cmPAvgSum = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	cl_mem cmPVarSum = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	/// 求均值
	clSetKernelArg(m_ckAvgGroupSum16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckAvgGroupSum16, 1, sizeof(cl_mem), (void*)&cmPAvgSum);
	clSetKernelArg(m_ckAvgGroupSum16, 2, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckAvgGroupSum16, 3, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckAvgGroupSum16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 2;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
    clSetKernelArg(m_ckDoubleGlobalSum, 0, sizeof(cl_mem), (void*)&cmPAvgSum);
    if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalSum, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);	
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPAvgSum, CL_TRUE, 0, 1 * sizeof(double), &dAvg, 0, NULL, NULL) != CL_SUCCESS)	return 4;
	dAvg /= pixels;
	/// 求方差
	clSetKernelArg(m_ckVarGroupSum16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckVarGroupSum16, 1, sizeof(double), &dAvg);
	clSetKernelArg(m_ckVarGroupSum16, 2, sizeof(cl_mem), (void*)&cmPVarSum);
	clSetKernelArg(m_ckVarGroupSum16, 3, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckVarGroupSum16, 4, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckVarGroupSum16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 5;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	clSetKernelArg(m_ckDoubleGlobalSum, 0, sizeof(cl_mem), (void*)&cmPVarSum);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalSum, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 6;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPVarSum, CL_TRUE, 0, 1 * sizeof(double), &dVar, 0, NULL, NULL) != CL_SUCCESS)	return 7;
 	dVar = dVar / pixels;

	*pdAvg = dAvg;
	*pdStdev = sqrt(dVar);
	
	if (cmPAvgSum)   clReleaseMemObject(cmPAvgSum);
	if (cmPVarSum)   clReleaseMemObject(cmPVarSum);
	
	return 0;
}

int ImageProcAlgo::AvgStdGPU8(cl_mem cmInput, double* pdAvg, double* pdStdev, size_t szImageWidth, size_t szImageHeight)
{
	double dAvg = 0;
	double dVar = 0;
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szWorkGroup = DivUp(szImageWidth, szLocalWorkSize);
	size_t szGlobalWorkSize = (szWorkGroup * szLocalWorkSize);
	size_t pixels = szImageWidth * szImageHeight;
	cl_mem cmPAvgSum = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	cl_mem cmPVarSum = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	/// 求均值
	clSetKernelArg(m_ckAvgGroupSum8, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckAvgGroupSum8, 1, sizeof(cl_mem), (void*)&cmPAvgSum);
	clSetKernelArg(m_ckAvgGroupSum8, 2, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckAvgGroupSum8, 3, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckAvgGroupSum8, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 2;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	clSetKernelArg(m_ckDoubleGlobalSum, 0, sizeof(cl_mem), (void*)&cmPAvgSum);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalSum, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPAvgSum, CL_TRUE, 0, 1 * sizeof(double), &dAvg, 0, NULL, NULL) != CL_SUCCESS)	return 4;
	dAvg /= pixels;
	/// 求方差
	clSetKernelArg(m_ckVarGroupSum8, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckVarGroupSum8, 1, sizeof(double), &dAvg);
	clSetKernelArg(m_ckVarGroupSum8, 2, sizeof(cl_mem), (void*)&cmPVarSum);
	clSetKernelArg(m_ckVarGroupSum8, 3, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckVarGroupSum8, 4, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckVarGroupSum8, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 5;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	clSetKernelArg(m_ckDoubleGlobalSum, 0, sizeof(cl_mem), (void*)&cmPVarSum);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalSum, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 6;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPVarSum, CL_TRUE, 0, 1 * sizeof(double), &dVar, 0, NULL, NULL) != CL_SUCCESS)	return 7;
	dVar = dVar / pixels;

	*pdAvg = dAvg;
	*pdStdev = sqrt(dVar);
	if (cmPAvgSum)   clReleaseMemObject(cmPAvgSum);
	if (cmPVarSum)   clReleaseMemObject(cmPVarSum);

	return 0;
}

int ImageProcAlgo::MinMaxGPU16(cl_mem cmInput, double* pdMin, double* pdMax, size_t szImageWidth, size_t szImageHeight)
{
	double dMin = 0;
	double dMax = 0;
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szWorkGroup = DivUp(szImageWidth, szLocalWorkSize);
	size_t szGlobalWorkSize = (szWorkGroup * szLocalWorkSize);
	size_t pixels = szImageWidth * szImageHeight;
	cl_mem cmPMinInGroup = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	cl_mem cmPMaxInGroup = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	
	clSetKernelArg(m_ckMinMaxGroup16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckMinMaxGroup16, 1, sizeof(cl_mem), (void*)&cmPMinInGroup);
	clSetKernelArg(m_ckMinMaxGroup16, 2, sizeof(cl_mem), (void*)&cmPMaxInGroup);
	clSetKernelArg(m_ckMinMaxGroup16, 3, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckMinMaxGroup16, 4, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckMinMaxGroup16, 5, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckMinMaxGroup16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 2;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	clSetKernelArg(m_ckDoubleGlobalMinMax, 0, sizeof(cl_mem), (void*)&cmPMinInGroup);
	clSetKernelArg(m_ckDoubleGlobalMinMax, 1, sizeof(cl_mem), (void*)&cmPMaxInGroup);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalMinMax, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPMinInGroup, CL_TRUE, 0, 1 * sizeof(double), &dMin, 0, NULL, NULL) != CL_SUCCESS)	return 4;
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPMaxInGroup, CL_TRUE, 0, 1 * sizeof(double), &dMax, 0, NULL, NULL) != CL_SUCCESS)	return 5;
	
	*pdMin = dMin;
	*pdMax = dMax;
	
	if (cmPMinInGroup)   clReleaseMemObject(cmPMinInGroup);
	if (cmPMaxInGroup)   clReleaseMemObject(cmPMaxInGroup);
	
	return 0;
}

int ImageProcAlgo::MinMaxGPU8(cl_mem cmInput, double* pdMin, double* pdMax, size_t szImageWidth, size_t szImageHeight)
{
	double dMin = 0;
	double dMax = 0;
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szWorkGroup = DivUp(szImageWidth, szLocalWorkSize);
	size_t szGlobalWorkSize = (szWorkGroup * szLocalWorkSize);
	size_t pixels = szImageWidth * szImageHeight;
	cl_mem cmPMinInGroup = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);
	cl_mem cmPMaxInGroup = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, szWorkGroup * sizeof(double), NULL, &ciErrNum);

	clSetKernelArg(m_ckMinMaxGroup8, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckMinMaxGroup8, 1, sizeof(cl_mem), (void*)&cmPMinInGroup);
	clSetKernelArg(m_ckMinMaxGroup8, 2, sizeof(cl_mem), (void*)&cmPMaxInGroup);
	clSetKernelArg(m_ckMinMaxGroup8, 3, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckMinMaxGroup8, 4, sizeof(cl_double), (void*)NULL);
	clSetKernelArg(m_ckMinMaxGroup8, 5, sizeof(unsigned int), (void*)&pixels);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckMinMaxGroup8, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 2;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	clSetKernelArg(m_ckDoubleGlobalMinMax, 0, sizeof(cl_mem), (void*)&cmPMinInGroup);
	clSetKernelArg(m_ckDoubleGlobalMinMax, 1, sizeof(cl_mem), (void*)&cmPMaxInGroup);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDoubleGlobalMinMax, 1, NULL, &szWorkGroup, NULL, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPMinInGroup, CL_TRUE, 0, 1 * sizeof(double), &dMin, 0, NULL, NULL) != CL_SUCCESS)	return 4;
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmPMaxInGroup, CL_TRUE, 0, 1 * sizeof(double), &dMax, 0, NULL, NULL) != CL_SUCCESS)	return 5;

	*pdMin = dMin;
	*pdMax = dMax;
	if (cmPMinInGroup)   clReleaseMemObject(cmPMinInGroup);
	if (cmPMaxInGroup)   clReleaseMemObject(cmPMaxInGroup);

	return 0;
}

int ImageProcAlgo::Short2ByteAutoGPU(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int* minv, int* maxv, double minscale, double maxscale, double lim, double thresh)
{
	size_t szGlobalWorkSize = szImageWidth * szImageHeight;
    size_t szLocalWorkSize = 32;
	cl_int ciErrNum = CL_SUCCESS;
	cl_mem cmHist = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, 65536 * sizeof(unsigned int), NULL, &ciErrNum);
	
	unsigned int* pauiHist = new unsigned int[65536];
	memset(pauiHist, 0, 65536 * sizeof(unsigned int));
	
	clSetKernelArg(m_ckHist16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckHist16, 1, sizeof(cl_mem), (void*)&cmHist);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckHist16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 4;
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	if (clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, cmHist, CL_TRUE, 0, 65536 * sizeof(unsigned int), pauiHist, 0, NULL, NULL) != CL_SUCCESS)	return 5;

	int iLimit, iThreshold;
	size_t pixels = szImageWidth * szImageHeight;
	if (lim < 0)
		iLimit = (int)((pixels) / 10);
	else
		iLimit = (int)((pixels) / lim);

	if (thresh < 0)
        iThreshold = (int)((pixels) / 7000);
	else
		iThreshold = (int)((pixels) / thresh);
	/// 重新计算有效色阶范围
    int i = 300;    // 1024*1024,<300,wrong
	long counter = 0;
	bool found1 = false;
	do {
		i++;
		counter = pauiHist[i];
		found1 = counter > iThreshold;
	} while ((!found1) && (i < 65535));
	int min0 = (int)(i * minscale);

	i = 65535;
	do {
		i--;
		counter = pauiHist[i];
		found1 = counter > iThreshold;
	} while ((!found1) && (i > 0));
	int max0 = (int)(i * maxscale);
	/// 根据色阶范围作映射
	float diff = (float)(max0 - min0);

	*minv = min0;
	*maxv = max0;

	clSetKernelArg(m_ckShort2Byte, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckShort2Byte, 1, sizeof(int), &min0);
	clSetKernelArg(m_ckShort2Byte, 2, sizeof(int), &max0);
	clSetKernelArg(m_ckShort2Byte, 3, sizeof(float), &diff);
	clSetKernelArg(m_ckShort2Byte, 4, sizeof(cl_mem), (void*)&cmOutput);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckShort2Byte, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 6;
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	if (cmHist)	clReleaseMemObject(cmHist);
	if (pauiHist)	delete[] pauiHist;

	return 0;
}

int ImageProcAlgo::Short2ByteMinMaxGPU(cl_mem cmInput, cl_mem cmOutput, int minv, int maxv, size_t szImageWidth, size_t szImageHeight)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szGlobalWorkSize = szImageWidth * szImageHeight;
	float fDiff = maxv - minv;
	clSetKernelArg(m_ckShort2Byte, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckShort2Byte, 1, sizeof(int), &minv);
	clSetKernelArg(m_ckShort2Byte, 2, sizeof(int), &maxv);	
	clSetKernelArg(m_ckShort2Byte, 3, sizeof(float), &fDiff);
	clSetKernelArg(m_ckShort2Byte, 4, sizeof(cl_mem), (void*)&cmOutput);
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckShort2Byte, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::SubBG16(cl_mem cmInput, cl_mem cmOutput, float fSigma2d, float fAvr, float fStd, float fRatioStd, size_t szImageWidth, size_t szImageHeight)
{
    std::chrono::high_resolution_clock::time_point beginTime;
    std::chrono::high_resolution_clock::time_point endTime;
    std::chrono::milliseconds timeInterval;

//    int iHSize = int(ceil(3 * fSigma2d) * 2 + 1);
    int iHSize = int(ceil(3 * 1.5) * 2 + 1);
	cl_int ciErrNum = CL_SUCCESS;
	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);		
	clEnqueueCopyBuffer(m_pGpuDevMngr->cqCommandQueue, cmInput, m_cmBg1, 0, 0, szImageWidth * szImageHeight * sizeof(unsigned short), 0, NULL, NULL);		
    /// 去饱和
    size_t szLocalWorkSize = 32;
    size_t szGlobalWorkSize = szImageWidth * szImageHeight;
    clSetKernelArg(m_ckReduceSaturation16, 0, sizeof(cl_mem), (void*)&m_cmBg1);
    clSetKernelArg(m_ckReduceSaturation16, 1, sizeof(cl_mem), (void*)&m_cmBg1);
    clSetKernelArg(m_ckReduceSaturation16, 2, sizeof(float), &fAvr);
    clSetKernelArg(m_ckReduceSaturation16, 3, sizeof(float), &fStd);
    clSetKernelArg(m_ckReduceSaturation16, 4, sizeof(float), &fRatioStd);
    if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckReduceSaturation16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 3;
    clFinish(m_pGpuDevMngr->cqCommandQueue);
	/// 高斯滤波
	clSetKernelArg(m_ckGaussianFilter16, 0, sizeof(cl_mem), (void*)&m_cmBg1);
	clSetKernelArg(m_ckGaussianFilter16, 1, sizeof(cl_mem), (void*)&m_cmBg2);
	clSetKernelArg(m_ckGaussianFilter16, 2, sizeof(unsigned int), &szImageWidth);
	clSetKernelArg(m_ckGaussianFilter16, 3, sizeof(unsigned int), &szImageHeight);
	clSetKernelArg(m_ckGaussianFilter16, 4, sizeof(float), &fSigma2d);
	clSetKernelArg(m_ckGaussianFilter16, 5, sizeof(unsigned int), &iHSize);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGaussianFilter16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);	
	/// 帧差	
	int iCenterShiftX12 = 0;
	int iCenterShiftY12 = 0;
	clSetKernelArg(m_ckSubFrame16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckSubFrame16, 1, sizeof(cl_mem), (void*)&m_cmBg2);
	clSetKernelArg(m_ckSubFrame16, 2, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckSubFrame16, 3, sizeof(int), &szImageWidth);
	clSetKernelArg(m_ckSubFrame16, 4, sizeof(int), &szImageHeight);
	clSetKernelArg(m_ckSubFrame16, 5, sizeof(int), &iCenterShiftX12);
	clSetKernelArg(m_ckSubFrame16, 6, sizeof(int), &iCenterShiftY12);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckSubFrame16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::Crop16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, size_t szCropWidth, size_t szCropHeight)
{
	cl_int ciErrNum = CL_SUCCESS;

	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
	int iWidthL = (szImageWidth - szCropWidth) / 2;
	int iWidthR = iWidthL + szCropWidth - 1;
	int iHeightU = (szImageHeight - szCropHeight) / 2;
	int iHeightD = iHeightU + szCropHeight - 1;
	clSetKernelArg(m_ckCrop16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckCrop16, 1, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckCrop16, 2, sizeof(int), &szImageWidth);
	clSetKernelArg(m_ckCrop16, 3, sizeof(int), &szImageHeight);
	clSetKernelArg(m_ckCrop16, 4, sizeof(int), &iWidthL);
	clSetKernelArg(m_ckCrop16, 5, sizeof(int), &iWidthR);
	clSetKernelArg(m_ckCrop16, 6, sizeof(int), &iHeightU);
	clSetKernelArg(m_ckCrop16, 7, sizeof(int), &iHeightD);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckCrop16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);	

	return 0;
}

int ImageProcAlgo::Binary16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, unsigned short usThresh)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t szLocalWorkSize = 32;
	size_t szGlobalWorkSize = szImageWidth * szImageHeight;

	clSetKernelArg(m_ckBinary16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckBinary16, 1, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckBinary16, 2, sizeof(unsigned short), &usThresh);	
	if (clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckBinary16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL) != CL_SUCCESS)	return 3;
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::SubFrameNoStar(cl_mem cmInput1, cl_mem cmInput2, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize, int iErodeSize, pair<float, float> pairfStarSpd)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
	/// 灰度膨胀
	clSetKernelArg(m_ckGrayDilate16, 0, sizeof(cl_mem), (void*)&cmInput2);
	clSetKernelArg(m_ckGrayDilate16, 1, sizeof(cl_mem), (void*)&m_cmDilate);
	clSetKernelArg(m_ckGrayDilate16, 2, sizeof(unsigned int), &szImageWidth);
	clSetKernelArg(m_ckGrayDilate16, 3, sizeof(unsigned int), &szImageHeight);
	clSetKernelArg(m_ckGrayDilate16, 4, sizeof(unsigned int), &iDilateSize);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGrayDilate16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);
	/// 判断是否为恒动模式
	if (pairfStarSpd.first == 0 && pairfStarSpd.second == 0)
	{
		/// 帧差
		int iCenterShiftX12 = (int)-pairfStarSpd.first;
		int iCenterShiftY12 = (int)-pairfStarSpd.second;
		clSetKernelArg(m_ckSubFrame16, 0, sizeof(cl_mem), (void*)&cmInput1);
		clSetKernelArg(m_ckSubFrame16, 1, sizeof(cl_mem), (void*)&m_cmDilate);
		clSetKernelArg(m_ckSubFrame16, 2, sizeof(cl_mem), (void*)&m_cmSubDilate);
		clSetKernelArg(m_ckSubFrame16, 3, sizeof(int), &szImageWidth);
		clSetKernelArg(m_ckSubFrame16, 4, sizeof(int), &szImageHeight);
		clSetKernelArg(m_ckSubFrame16, 5, sizeof(int), &iCenterShiftX12);
		clSetKernelArg(m_ckSubFrame16, 6, sizeof(int), &iCenterShiftY12);
		clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckSubFrame16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
		clFinish(m_pGpuDevMngr->cqCommandQueue);
		/// 灰度腐蚀
		clSetKernelArg(m_ckGrayErode16, 0, sizeof(cl_mem), (void*)&m_cmSubDilate);
		clSetKernelArg(m_ckGrayErode16, 1, sizeof(cl_mem), (void*)&cmOutput);
		clSetKernelArg(m_ckGrayErode16, 2, sizeof(unsigned int), &szImageWidth);
		clSetKernelArg(m_ckGrayErode16, 3, sizeof(unsigned int), &szImageHeight);
		clSetKernelArg(m_ckGrayErode16, 4, sizeof(unsigned int), &iDilateSize);
		clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGrayErode16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
		clFinish(m_pGpuDevMngr->cqCommandQueue);
	} 
	else
	{
		/// 帧差
		int iCenterShiftX12 = (int)-pairfStarSpd.first;
		int iCenterShiftY12 = (int)-pairfStarSpd.second;
		clSetKernelArg(m_ckSubFrame16, 0, sizeof(cl_mem), (void*)&cmInput1);
		clSetKernelArg(m_ckSubFrame16, 1, sizeof(cl_mem), (void*)&m_cmDilate);
		clSetKernelArg(m_ckSubFrame16, 2, sizeof(cl_mem), (void*)&cmOutput);
		clSetKernelArg(m_ckSubFrame16, 3, sizeof(int), &szImageWidth);
		clSetKernelArg(m_ckSubFrame16, 4, sizeof(int), &szImageHeight);
		clSetKernelArg(m_ckSubFrame16, 5, sizeof(int), &iCenterShiftX12);
		clSetKernelArg(m_ckSubFrame16, 6, sizeof(int), &iCenterShiftY12);
		clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckSubFrame16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
		clFinish(m_pGpuDevMngr->cqCommandQueue);
	}	
	
	return 0;
}

int ImageProcAlgo::MedianFilter16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iHSize)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
	/// 中值滤波
	clSetKernelArg(m_ckMedianFilter16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckMedianFilter16, 1, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckMedianFilter16, 2, sizeof(unsigned int), &szImageWidth);
	clSetKernelArg(m_ckMedianFilter16, 3, sizeof(unsigned int), &szImageHeight);
	clSetKernelArg(m_ckMedianFilter16, 4, sizeof(unsigned int), &iHSize);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckMedianFilter16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::SaltFilter8(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
	/// 中值滤波
	clSetKernelArg(m_ckSaltFilter8, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckSaltFilter8, 1, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckSaltFilter8, 2, sizeof(unsigned int), &szImageWidth);
	clSetKernelArg(m_ckSaltFilter8, 3, sizeof(unsigned int), &szImageHeight);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckSaltFilter8, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::Binning16(cl_mem cmInput, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iBinning)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t paszLocalWorkSize[2];
	paszLocalWorkSize[0] = 16;
	paszLocalWorkSize[1] = 16;
	size_t paszGlobalWorkSize[2];
	paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth / iBinning);
	paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight / iBinning);
	/// 中值滤波
	clSetKernelArg(m_ckBinning16, 0, sizeof(cl_mem), (void*)&cmInput);
	clSetKernelArg(m_ckBinning16, 1, sizeof(cl_mem), (void*)&cmOutput);
	clSetKernelArg(m_ckBinning16, 2, sizeof(unsigned int), &szImageWidth);
	clSetKernelArg(m_ckBinning16, 3, sizeof(unsigned int), &szImageHeight);
	clSetKernelArg(m_ckBinning16, 4, sizeof(unsigned int), &iBinning);
	clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckBinning16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::Median5FramesGPU(cl_mem cmIn1, cl_mem cmIn2, cl_mem cmIn3, cl_mem cmIn4, cl_mem cmIn5, cl_mem cmOut, size_t szImageWidth, size_t szImageHeight)
{
	cl_int ciErrNum = CL_SUCCESS;
	size_t szGlobalWorkSize = (szImageWidth * szImageHeight);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 0, sizeof(cl_mem), (void*)&cmIn1);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 1, sizeof(cl_mem), (void*)&cmIn2);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 2, sizeof(cl_mem), (void*)&cmIn3);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 3, sizeof(cl_mem), (void*)&cmIn4);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 4, sizeof(cl_mem), (void*)&cmIn5);
	ciErrNum = clSetKernelArg(m_ckFrame5Median16, 5, sizeof(cl_mem), (void*)&cmOut);
	ciErrNum = clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckFrame5Median16, 1, NULL, &szGlobalWorkSize, NULL, 0, NULL, NULL);
	clFinish(m_pGpuDevMngr->cqCommandQueue);

	return 0;
}

int ImageProcAlgo::Rotate16(cl_mem cmIn, cl_mem cmOut1, cl_mem cmOut2, size_t szImageWidth, size_t szImageHeight, bool bRotate, double dAvg, double dStd, bool bNoSatuZone)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ck_Rotate16, 0, sizeof(cl_mem), (void*)&cmIn);
    clSetKernelArg(m_ck_Rotate16, 1, sizeof(cl_mem), (void*)&cmOut1);
    clSetKernelArg(m_ck_Rotate16, 2, sizeof(cl_mem), (void*)&cmOut2);
    clSetKernelArg(m_ck_Rotate16, 3, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ck_Rotate16, 4, sizeof(unsigned int), &szImageHeight);
    unsigned int uiRotate = bRotate ? 1 : 0;
    clSetKernelArg(m_ck_Rotate16, 5, sizeof(unsigned int), &uiRotate);
    clSetKernelArg(m_ck_Rotate16, 6, sizeof(double), &dAvg);
    clSetKernelArg(m_ck_Rotate16, 7, sizeof(double), &dStd);
    unsigned int uiNoSatuZone = bNoSatuZone ? 1 : 0;
    clSetKernelArg(m_ck_Rotate16, 8, sizeof(unsigned int), &uiNoSatuZone);
    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ck_Rotate16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

void ImageProcAlgo::CropPos2RealPos(pair<float, float> pairfCropPos, pair<float, float>& pairfRealPos, size_t szImageWidth, size_t szImageHeight, size_t szCropWidth, size_t szCropHeight)
{
	pairfRealPos.first = pairfCropPos.first + (szImageWidth - szCropWidth) / 2;
    pairfRealPos.second = pairfCropPos.second + (szImageHeight - szCropHeight) / 2;
}

void ImageProcAlgo::LoadBadPixel(QString qstrFileName)
{
    QFile file(qstrFileName);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        while(!file.atEnd())
        {
            QByteArray line = file.readLine();
            QString strLine(line);
            QStringList strList = strLine.split(",");
            if (strList.size() == 2)
            {
                pair<float, float> pairfBadPixel(strList[0].toFloat(), strList[1].toFloat());
                m_vectpairfBadPixel.push_back(pairfBadPixel);
            }
        }
        file.close();
    }
}

int ImageProcAlgo::RemoveBadLine(cl_mem cmIn, cl_mem cmOut, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ckRemoveBadLine, 0, sizeof(cl_mem), (void*)&cmIn);
    clSetKernelArg(m_ckRemoveBadLine, 1, sizeof(cl_mem), (void*)&cmOut);
    clSetKernelArg(m_ckRemoveBadLine, 2, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckRemoveBadLine, 3, sizeof(unsigned int), &szImageHeight);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckRemoveBadLine, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

void ImageProcAlgo::RemoveBadPixel(Blobs &blobs)
{
    for (int j = 0; j < m_vectpairfBadPixel.size(); j++)
    {
        for (Blobs::iterator iterB = blobs.begin(); iterB != blobs.end(); iterB++)
        {
            pair<float, float> pairfBlobPos;
            pairfBlobPos = pair<float, float>((*iterB).second->centroid_x * m_iBinning, (*iterB).second->centroid_y * m_iBinning);
            if ((abs(pairfBlobPos.first - m_vectpairfBadPixel[j].first) < 10.0
                    && abs(pairfBlobPos.second - m_vectpairfBadPixel[j].second) < 10.0))
            {
                Blob* pBlob = (*iterB).second;
                delete pBlob;
                Blobs::iterator tmp = iterB;
                blobs.erase(tmp);
//                break;
            }
        }
    }
}

void ImageProcAlgo::RemoveBadBlob(Blobs &blobs)
{
    vector<int> vectiSeq;
    for (Blobs::iterator iterB = blobs.begin(); iterB != blobs.end(); iterB++)
    {
        pair<float, float> pairfBlobPos;
        pairfBlobPos = pair<float, float>((*iterB).second->centroid_x * m_iBinning, (*iterB).second->centroid_y * m_iBinning);
        if (pairfBlobPos.first < 100 || m_pGParam->m_SGrabberData.iFullWidth - pairfBlobPos.first < 100
          || pairfBlobPos.second < 100 || m_pGParam->m_SGrabberData.iFullHeight - pairfBlobPos.second < 100)
        {
            vectiSeq.push_back(iterB->first);
        }
    }
    for (int j = 0; j < vectiSeq.size(); j++)
    {
        for (Blobs::iterator iterB = blobs.begin(); iterB != blobs.end(); iterB++)
        {
            if ((*iterB).first == vectiSeq[j])
            {
                Blob* pBlob = (*iterB).second;
                delete pBlob;
                Blobs::iterator tmp = iterB;
                blobs.erase(tmp);
//                break;
            }
        }
    }
}

int ImageProcAlgo::DilateGray16(cl_mem cmInput1, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
    /// 灰度膨胀
    clSetKernelArg(m_ckGrayDilate16, 0, sizeof(cl_mem), (void*)&cmInput1);
    clSetKernelArg(m_ckGrayDilate16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckGrayDilate16, 2, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckGrayDilate16, 3, sizeof(unsigned int), &szImageHeight);
    clSetKernelArg(m_ckGrayDilate16, 4, sizeof(unsigned int), &iDilateSize);
    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGrayDilate16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::DilateGray8(cl_mem cmInput1, cl_mem cmOutput, size_t szImageWidth, size_t szImageHeight, int iDilateSize)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);
    /// 灰度膨胀
    clSetKernelArg(m_ckGrayDilate8, 0, sizeof(cl_mem), (void*)&cmInput1);
    clSetKernelArg(m_ckGrayDilate8, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckGrayDilate8, 2, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckGrayDilate8, 3, sizeof(unsigned int), &szImageHeight);
    clSetKernelArg(m_ckGrayDilate8, 4, sizeof(unsigned int), &iDilateSize);
    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGrayDilate8, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo)
{
//	vectTargetInfo = m_vectTargetInfo;
    m_pTrakcer->GetVectTargetInfo(vectTargetInfo);

	return 0;
}

int ImageProcAlgo::GetTargetInfo(sTargetInfo& targetInfo)
{
//	targetInfo = m_targetInfo;
    m_pTrakcer->GetTargetInfo(targetInfo);

    return 0;
}

int ImageProcAlgo::InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID)
{
    m_ulFrameSeq = 0;
    m_iNumBlob = 0;
    m_iTrackMode = iTrackMode;
    m_trackSettings = settings;
    m_pTrakcer->InitTracker(iTrackMode, settings, iTargetID);
    InitData();
    {
        for (vector<sFramePacket>::iterator iter = m_vectFramePacket.begin(); iter != m_vectFramePacket.end(); iter++)
        {
            if ((unsigned short*)((*iter).sausImageGrab.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageGrab.pvoidBuffer);
            if ((unsigned short*)((*iter).sausImageEnhance.pvoidBuffer))	delete[](unsigned short*)((*iter).sausImageEnhance.pvoidBuffer);
            if ((float*)((*iter).safImagePhotometry.pvoidBuffer))	delete[](float*)((*iter).safImagePhotometry.pvoidBuffer);
        }
    }
    m_vectFramePacket.clear();
}

size_t ImageProcAlgo::RoundUp(int group_size, int global_size)
{
	int r = global_size % group_size;
	if (r == 0) 
	{
		return global_size;
	}
	else 
	{
		return global_size + group_size - r;
    }
}

bool ImageProcAlgo::ReadFlatImage(QString qstrFileName)
{
    cl_int ciErrNum = CL_SUCCESS;

    QFile qfileRead(qstrFileName);
    qfileRead.open(QIODevice::ReadOnly);
    if (qfileRead.exists())
    {
        m_pausFlatFull = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pausFlatFull, 1, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
        qfileRead.read((char*)m_pausFlatFull, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
        m_cmFlat = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short), NULL, &ciErrNum);
        ciErrNum = clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmFlat, CL_TRUE, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short), m_pausFlatFull, 0, NULL, NULL);

        double dTemp;
        AvgStdGPU16(m_cmFlat, &m_dFlatAvrFull, &dTemp, m_pGParam->m_SGrabberData.iFullWidth, m_pGParam->m_SGrabberData.iFullHeight);

        m_pausFlatCrop = new unsigned short[m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_pausFlatCrop, 1, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short));
        unsigned int uiStartColumn = m_pGParam->m_SGrabberData.iSubInFullCenterX - m_pGParam->m_SGrabberData.iSubWidth/2;
        unsigned int uiStartRow = m_pGParam->m_SGrabberData.iSubInFullCenterY - m_pGParam->m_SGrabberData.iSubHeight/2;
        for (int i = 0; i < m_pGParam->m_SGrabberData.iSubHeight; i++)
        {
            memcpy(m_pausFlatCrop + i * m_pGParam->m_SGrabberData.iSubWidth, m_pausFlatFull + (i + uiStartRow) * m_pGParam->m_SGrabberData.iFullWidth + uiStartColumn, m_pGParam->m_SGrabberData.iSubWidth * sizeof(unsigned short));
        }
        m_cmFlatCrop = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short), NULL, &ciErrNum);
        clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmFlatCrop, CL_TRUE, 0, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short), m_pausFlatCrop, 0, NULL, NULL);
        AvgStdGPU16(m_cmFlatCrop, &m_dFlatAvrCrop, &dTemp, m_pGParam->m_SGrabberData.iSubWidth, m_pGParam->m_SGrabberData.iSubHeight);

        return true;
    }

    return false;
}

bool ImageProcAlgo::ReadBiasImage(QString qstrFileName)
{
    cl_int ciErrNum = CL_SUCCESS;

    QFile qfileRead(qstrFileName);
    qfileRead.open(QIODevice::ReadOnly);
    if (qfileRead.exists())
    {
        m_pausBiasFull = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pausBiasFull, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
        qfileRead.read((char*)m_pausBiasFull, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
        m_cmBias = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short), NULL, &ciErrNum);
        clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmBias, CL_TRUE, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short), m_pausBiasFull, 0, NULL, NULL);

        m_pausBiasCrop = new unsigned short[m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_pausBiasCrop, 0, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short));
        unsigned int uiStartColumn = m_pGParam->m_SGrabberData.iSubInFullCenterX - m_pGParam->m_SGrabberData.iSubWidth/2;
        unsigned int uiStartRow = m_pGParam->m_SGrabberData.iSubInFullCenterY - m_pGParam->m_SGrabberData.iSubHeight/2;
        for (int i = 0; i < m_pGParam->m_SGrabberData.iSubHeight; i++)
        {
            memcpy(m_pausBiasCrop + i * m_pGParam->m_SGrabberData.iSubWidth, m_pausBiasFull + (i + uiStartRow) * m_pGParam->m_SGrabberData.iFullWidth + uiStartColumn, m_pGParam->m_SGrabberData.iSubWidth * sizeof(unsigned short));
        }
        m_cmBiasCrop = clCreateBuffer(m_pGpuDevMngr->cxGPUContext, CL_MEM_READ_WRITE, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short), NULL, &ciErrNum);
        clEnqueueWriteBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmBiasCrop, CL_TRUE, 0, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short), m_pausBiasCrop, 0, NULL, NULL);

        return true;
    }

    return false;
}

int ImageProcAlgo::FlatBiasCorrect(cl_mem cmInput, cl_mem cmOutput, cl_mem cmFlat, cl_mem cmBias, double dFlatAvr, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ckFlatBiasCorrect, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckFlatBiasCorrect, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckFlatBiasCorrect, 2, sizeof(cl_mem), (void*)&cmFlat);
    clSetKernelArg(m_ckFlatBiasCorrect, 3, sizeof(cl_mem), (void*)&cmBias);
    clSetKernelArg(m_ckFlatBiasCorrect, 4, sizeof(double), &dFlatAvr);
    clSetKernelArg(m_ckFlatBiasCorrect, 5, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckFlatBiasCorrect, 6, sizeof(unsigned int), &szImageHeight);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckFlatBiasCorrect, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

bool ImageProcAlgo::CalcTargetInSubZone(unsigned short *pausInput, pair<float,float> pairfFOVCenterAE, pair<float, float> pairfSelectPos, sMeasureBlob& blob, float fThresh1, float fThresh2)
{
    int iSelectPosX = (int)pairfSelectPos.first;
    int iSelectPosY = (int)pairfSelectPos.second;
    int iSelectZoneLeft = iSelectPosX - 40;
    int iSelectZoneRight = iSelectPosX + 40;
    int iSelectZoneUp = iSelectPosY - 40;
    int iSelectZoneDown = iSelectPosY + 40;

    if (iSelectZoneLeft < 0)
    {
        iSelectZoneLeft = 0;
    }
    if (iSelectZoneRight > m_szImageWidth - 1)
    {
        iSelectZoneRight = m_szImageWidth - 1;
    }
    if (iSelectZoneUp < 0)
    {
        iSelectZoneUp = 0;
    }
    if (iSelectZoneDown > m_szImageHeight - 1)
    {
        iSelectZoneDown = m_szImageHeight - 1;
    }

    unsigned int uiSubWidth = iSelectZoneRight - iSelectZoneLeft + 1;
    unsigned int uiSubHeight = iSelectZoneDown - iSelectZoneUp + 1;
    unsigned short* pausSub = new unsigned short[uiSubWidth * uiSubHeight];
    memset(pausSub, 0, uiSubWidth * uiSubHeight * sizeof(unsigned short));

//    #pragma omp parallel for num_threads(8)
    for (int i = iSelectZoneUp; i <= iSelectZoneDown; i++)
    {
        memcpy(pausSub + (i - iSelectZoneUp) * uiSubWidth, pausInput + (i * m_szImageWidth + iSelectZoneLeft), uiSubWidth * sizeof(unsigned short));
    }

    float fAvg = 0;
    float fRms = 0;
    for (int i = 0; i < uiSubHeight; i++)
    {
        for (int j = 0; j < uiSubWidth; j++)
        {
            float fVal = (float)pausSub[i * uiSubWidth + j];
            fAvg += fVal;
            fRms += fVal * fVal;
        }
    }
    fAvg /= (uiSubWidth * uiSubHeight);
    fRms = fRms / (uiSubWidth * uiSubHeight) - fAvg * fAvg;
    if (fRms < 1)
    {
        if (pausSub)    delete [] pausSub;
        return  false;
    }

    double threshval = fAvg + fThresh1 * sqrt(fRms);
    fAvg = 0;
    fRms = 0;
    int bgcount = 0;
    for (int i = 0; i < uiSubHeight; i++)
    {
        for (int j = 0; j < uiSubWidth; j++)
        {
            float fVal = (float)pausSub[i * uiSubWidth + j];
            if (fVal < threshval)
            {
                fAvg += fVal;
                fRms += fVal * fVal;
                bgcount++;
            }
        }
    }
    fAvg /= (bgcount);
    fRms = fRms / bgcount - fAvg * fAvg;
    if (fRms < 0)
    {
        if (pausSub)    delete [] pausSub;
        return false;
    }

    threshval = fAvg + fThresh2 * sqrt(fRms);
    float fBgAvg = fAvg;
    float fBgRms = fRms;
    fAvg = 0;
    fRms = 0;
    int starcount = 0;
    double xt = 0;
    double yt = 0;
    double grey = 0;
    for (int i = 0; i < uiSubHeight; i++)
    {
        for (int j = 0; j < uiSubWidth; j++)
        {
            unsigned short val = pausInput[i * uiSubWidth + j];
            if (val >= threshval)
            {
                fAvg += val - fBgAvg;
                fRms += (val - fBgAvg) * (val - fBgAvg);
                grey += val;
                xt += (val)* j;
                yt += (val)* i;
                starcount++;
            }
        }
    }
    fAvg /= starcount;

    sMeasureBlob blobTemp;
    blobTemp.pairfPos = pair<float, float>(xt/grey, yt/grey); // cx,cy: 质心;centroid_x,centroid_y: 形心
    blobTemp.fMinX = blobTemp.pairfPos.first - 5;
    blobTemp.fMinY = blobTemp.pairfPos.second - 5;
    blobTemp.fMaxX = blobTemp.pairfPos.first + 5;
    blobTemp.fMaxY = blobTemp.pairfPos.second + 5;
    blobTemp.fArea = 96;

    blob.pairfPos = pair<float, float>(iSelectZoneLeft + blobTemp.pairfPos.first,
                                       iSelectZoneUp + blobTemp.pairfPos.second);
    blob.fMinX = iSelectZoneLeft + blobTemp.fMinX;
    blob.fMinY = iSelectZoneUp + blobTemp.fMinY;
    blob.fMaxX = iSelectZoneLeft + blobTemp.fMaxX;
    blob.fMaxY = iSelectZoneUp + blobTemp.fMaxY;
    blob.fArea = blobTemp.fArea;
    blob.pairfPosAE.second = pairfFOVCenterAE.second + (m_trackSettings.opticparams.fFOVCenterY - blob.pairfPos.second) * m_trackSettings.opticparams.fPixelScale;
    blob.pairfPosAE.first = pairfFOVCenterAE.first + (blob.pairfPos.first - m_trackSettings.opticparams.fFOVCenterX) * m_trackSettings.opticparams.fPixelScale / cos(blob.pairfPosAE.second / 180.0 * 3.1415926);

//    FILE* fp;
//    stringstream ss;
//    ss << "/home/dps/fffffff" << ".raw";
//    string  strFileName = ss.str();
//    fp =  fopen(strFileName.c_str(), "wb");
//    fwrite(pausSub, sizeof(unsigned short), uiSubWidth * uiSubHeight, fp);
//    fclose(fp);

    if (pausSub)    delete [] pausSub;

    return true;
}

void ImageProcAlgo::ClockOn()
{
    m_beginTime = std::chrono::high_resolution_clock::now();
}

int ImageProcAlgo::ClockOff()
{
    m_endTime = std::chrono::high_resolution_clock::now();	///////////////////////////
    return (std::chrono::duration_cast<std::chrono::milliseconds>(m_endTime - m_beginTime)).count();	///////////////////////////
}

int ImageProcAlgo::LocalContrastMeasure(cl_mem cmInput, cl_mem cmOutput, int iSize, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ckLCM, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckLCM, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckLCM, 2, sizeof(unsigned int), &iSize);
    clSetKernelArg(m_ckLCM, 3, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckLCM, 4, sizeof(unsigned int), &szImageHeight);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckLCM, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::ReduceLargeDN16(cl_mem cmInput, cl_mem cmOutput, float fThresh, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t szLocalWorkSize = 32;
    size_t szGlobalWorkSize = szImageWidth * szImageHeight;
    clSetKernelArg(m_ckReduceLargeDN16, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckReduceLargeDN16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckReduceLargeDN16, 2, sizeof(float), &fThresh);
    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckReduceLargeDN16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::ReduceSmallDN16(cl_mem cmInput, cl_mem cmOutput, float fAvr, float fStd, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t szLocalWorkSize = 32;
    size_t szGlobalWorkSize = szImageWidth * szImageHeight;
    clSetKernelArg(m_ckReduceSmallDN16, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckReduceSmallDN16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckReduceSmallDN16, 2, sizeof(float), &fAvr);
    clSetKernelArg(m_ckReduceSmallDN16, 3, sizeof(float), &fStd);
    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckReduceSmallDN16, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::Patch16(cl_mem cmInput, cl_mem cmOutput, int r, int c)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], 1024);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], 1024);

    clSetKernelArg(m_ckPatch16, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckPatch16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckPatch16, 2, sizeof(unsigned int), &r);
    clSetKernelArg(m_ckPatch16, 3, sizeof(unsigned int), &c);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckPatch16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::DePatch8(cl_mem cmInput, cl_mem cmOutput, int r, int c)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], 1024);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], 1024);

    clSetKernelArg(m_ckDePatch8, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckDePatch8, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckDePatch8, 2, sizeof(unsigned int), &r);
    clSetKernelArg(m_ckDePatch8, 3, sizeof(unsigned int), &c);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDePatch8, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::DePatch16(cl_mem cmInput, cl_mem cmOutput, int r, int c)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], 1024);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], 1024);

    clSetKernelArg(m_ckDePatch16, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckDePatch16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckDePatch16, 2, sizeof(unsigned int), &r);
    clSetKernelArg(m_ckDePatch16, 3, sizeof(unsigned int), &c);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckDePatch16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

size_t ImageProcAlgo::DivUp(size_t dividend, size_t divisor)
{
	return (dividend % divisor == 0) ? (dividend / divisor) : (dividend / divisor + 1);
}


//*************************************************************//
/// 蒙气差修正值计算 - 视高度角 - 张晓明提供 (输入:视高度角; 输出:真实高度角; 真实高度角<视高度角; 返回值>0; 真实高度角=视高度角-返回值)
double ImageProcAlgo::RefractVisual(const double vAlt, const double pressure, const double temperature)
{
    double arcdeg = vAlt + 7.31 * GtoR / (vAlt * RtoG + 4.4);
    double R = cot(arcdeg) * GtoR / 60;
    double scale = pressure * 283 / 1010 / (273 + temperature);
    return scale * R;
}

void ImageProcAlgo::TimeAddSeconds(int iYear, int iMonth, int iDay, int iHour, int iMinute, double dSecond, double dSecondAdd, int &iYearNew, int &iMonthNew, int &iDayNew, int &iHourNew, int &iMinuteNew, double &dSecondNew)
{
    iYearNew = iYear;
    iMonthNew = iMonth;
    iDayNew = iDay;
    iHourNew = iHour;
    iMinuteNew = iMinute;
    dSecondNew = dSecond + dSecondAdd;
    if (dSecondAdd > 0)
    {
        if (dSecondNew >= 60.0)
        {
            dSecondNew -= 60.0;
            iMinuteNew += 1;
            if (iMinuteNew >= 60)
            {
                iMinuteNew -= 60;
                iHourNew += 1;
                if (iHourNew >= 24)
                {
                    iHourNew -= 24;
                    iDayNew += 1;
                    if (iMonthNew == 1 || iMonthNew == 3 || iMonthNew == 5
                            || iMonthNew == 7 || iMonthNew == 8 || iMonthNew == 10 || iMonthNew == 12)
                    {
                        if (iDayNew > 31)
                        {
                            iDayNew -= 31;
                            if (iMonthNew == 12)
                            {
                                iMonthNew = 1;
                                iYearNew += 1;
                            }
                            else
                            {
                                iMonthNew += 1;
                            }
                        }
                    }
                    else if (iMonthNew == 2)
                    {
                        if ((iYearNew/4 == 0 && iYearNew/100 != 0)
                                || iYearNew/400 == 0)
                        {
                            if (iDayNew > 29)
                            {
                                iDayNew -= 29;
                                iMonthNew += 1;
                            }
                        }
                        else
                        {
                            if (iDayNew > 28)
                            {
                                iDayNew -= 28;
                                iMonthNew += 1;
                            }
                        }
                    }
                    else
                    {
                        if (iDayNew > 30)
                        {
                            iDayNew -= 30;
                            iMonthNew += 1;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (dSecondNew < 0)
        {
            dSecondNew += 60.0;
            iMinuteNew -= 1;
            if (iMinuteNew < 0)
            {
                iMinuteNew += 60;
                iHourNew -= 1;
                if (iHourNew < 0)
                {
                    iHourNew += 24;
                    iDayNew -= 1;
                    if (iMonthNew == 1 ||iMonthNew == 2 || iMonthNew == 4 || iMonthNew == 6
                            || iMonthNew == 8 || iMonthNew == 9 || iMonthNew == 11)
                    {
                        if (iDayNew < 0)
                        {
                            iDayNew = 31;
                            if (iMonthNew == 1)
                            {
                                iMonthNew = 12;
                                iYearNew -= 1;
                            }
                            else
                            {
                                iMonthNew -= 1;
                            }
                        }
                    }
                    else if (iMonthNew == 3)
                    {
                        if ((iYearNew/4 == 0 && iYearNew/100 != 0)
                                || iYearNew/400 == 0)
                        {
                            if (iDayNew < 0)
                            {
                                iDayNew = 29;
                                iMonthNew = 2;
                            }
                        }
                        else
                        {
                            if (iDayNew < 0)
                            {
                                iDayNew = 28;
                                iMonthNew = 2;
                            }
                        }
                    }
                    else
                    {
                        if (iDayNew < 0)
                        {
                            iDayNew = 30;
                            iMonthNew -= 1;
                        }
                    }
                }
            }
        }
    }
}

void ImageProcAlgo::LoadStarLib_TWDW()
{
    m_pstarmap = new CStarMap;

    char   strStarLibIdx[400];
    char   strStarLibCat[400];

//    sprintf(strStarLibIdx,"%s/DATA/GAIA_STAR_18.IDX","/home/dps/IniFiles");
//    sprintf(strStarLibCat,"%s/DATA/GAIA_STAR_18.CAT","/home/dps/IniFiles");

//    sprintf(strStarLibIdx,"%s/DATA/HIPO_STAR_11.IDX","/home/dps/IniFiles");
//    sprintf(strStarLibCat,"%s/DATA/HIPO_STAR_11.CAT","/home/dps/IniFiles");

//    sprintf(strStarLibIdx,"%s/DATA/TYCH_STAR_12.IDX","/home/dps/IniFiles");
//    sprintf(strStarLibCat,"%s/DATA/TYCH_STAR_12.CAT","/home/dps/IniFiles");

    sprintf(strStarLibIdx,"%s/DATA/GAIA_STAR_13.IDX","/home/dps/IniFiles");
    sprintf(strStarLibCat,"%s/DATA/GAIA_STAR_13.CAT","/home/dps/IniFiles");

    //初始化开始
    bool bStarLibLoad = false;
    if(m_pstarmap->Calc_LoadStarLib(strStarLibIdx,strStarLibCat))
    {
        qDebug()<<strStarLibIdx<<"does exist";
        qDebug()<<strStarLibCat<<"does exist";
        bStarLibLoad=true;
    }
    else
    {
        qDebug()<<strStarLibIdx<<"does not exist";
        qDebug()<<strStarLibCat<<"does not exist";
    }

    qDebug("恒星星库加载成功标志 %5d\n",bStarLibLoad);
}

void ImageProcAlgo::ChangeGAEStr(char *pacOldStr, char *pacNewStr, char* pacExpStr)
{
    QString qstrOldStr(pacOldStr);
    QStringList qstrlistOldStr = qstrOldStr.split(' ');
    qstrlistOldStr[3] = "2106";
    qstrlistOldStr[18] = "500";
    QString qstrNewStr = "";
    for (int i = 0; i < qstrlistOldStr.size(); i++)
    {
        if (qstrlistOldStr[i] == " ")
            qstrNewStr += " ";
        else
        {
            qstrNewStr += qstrlistOldStr[i];
            qstrNewStr += " ";
        }
    }
    memcpy(pacNewStr, qstrNewStr.toLocal8Bit().data(), qstrNewStr.size());
    memcpy(pacExpStr, qstrlistOldStr[11].toLocal8Bit().data(), qstrlistOldStr[11].size());
}

void ImageProcAlgo::InitData()
{
    if (m_vectTargetInfo.size() > 0)
        m_vectTargetInfo.clear();

    m_targetInfo.bLiving = false;
    m_targetInfo.fValid = 0;
    m_targetInfo.qstrTargetID = "";
    if (m_targetInfo.vectInfoInFrame.size() > 0)
        m_targetInfo.vectInfoInFrame.clear();
    if (m_targetInfo.vectResPacket.size() > 0)
        m_targetInfo.vectResPacket.clear();
    if (m_targetInfo.vectulFrameSeqTWDW.size() > 0)
        m_targetInfo.vectulFrameSeqTWDW.clear();
    if (m_targetInfo.vectulFrameSeqStore.size() > 0)
        m_targetInfo.vectulFrameSeqStore.clear();
    m_targetInfo.pairfPredPosAE = pair<float,float>(0,0);
    m_targetInfo.pairfPredSpdAE = pair<float,float>(0,0);
    m_targetInfo.pairfPredPosInFrame = pair<float,float>(0,0);
    m_targetInfo.pairfPredSpdInFrame = pair<float,float>(0,0);
    if (m_vectTargetInfo.size() > 0)
        m_vectpacketGAE.clear();
}

void ImageProcAlgo::WriteVedioTagFile(QString qstrPathName, QString qstrFileName, sMeasureBlob blob, QString qstrTargetID)
{
    QString qstrTotalName = qstrPathName + "/" + qstrFileName;

    /// 判断文件是否为空
    bool bEmpty;
    {
        vector<string> tmp_files;
        ifstream infile(qstrTotalName.toLocal8Bit().data());
        string lineContent;
        while ( getline( infile, lineContent, '\n' ) )
        {
            tmp_files.push_back(lineContent + "\n" );
        }
        infile.close();
        bEmpty = tmp_files.size() == 0;
    }

    if(bEmpty)  // 文件为first write，添加文件头与恒星数据
    {
        QString qstrTemp;
        FILE* fp = fopen(qstrTotalName.toLatin1().data(), "w");

        if (fp)
        {
            qstrTemp.sprintf("%d\r\n", 1);
            if (fp != NULL)  fwrite(qstrTemp.toLocal8Bit().data(), qstrTemp.size(), 1, fp);

            qstrTemp.sprintf("%.3f %.3f %.3f %.3f %.3f %.3f %06d",
                             blob.pairfPos.first, blob.pairfPos.second,
                             blob.fMinX, blob.fMaxX,
                             blob.fMinY, blob.fMaxY,
                             qstrTargetID.toUInt());

            if (fp != NULL)  fwrite(qstrTemp.toLocal8Bit().data(), qstrTemp.size(), 1, fp);
            if (fp != NULL)  fclose(fp);
        }
    }
    else
    {
        vector<string> tmp_files;
        ifstream infile(qstrTotalName.toLocal8Bit().data());
        string lineContent;
        while ( getline( infile, lineContent, '\n' ) )
        {
            tmp_files.push_back(lineContent + "\n" );
        }
        infile.close();

        /// 修改Number of Target
        unsigned int uiNumTarget = atoi(tmp_files[0].c_str()) + 1;
        tmp_files[0] = to_string(uiNumTarget) + "\r\n";

        /// Rewrite Pre Contest
        ofstream outfile(qstrTotalName.toLocal8Bit().data(), ios::trunc);
        copy(tmp_files.begin(), tmp_files.end(), ostream_iterator<string>(outfile));
        outfile.close();

        QString qstrTemp;
        FILE* fp = fopen(qstrTotalName.toLatin1().data(), "a");
        if (fp)
        {
            qstrTemp.sprintf("%.3f %.3f %.3f %.3f %.3f %.3f %06d",
                             blob.pairfPos.first, blob.pairfPos.second,
                             blob.fMinX, blob.fMaxX,
                             blob.fMinY, blob.fMaxY,
                             qstrTargetID.toUInt());

            if (fp != NULL)  fwrite(qstrTemp.toLocal8Bit().data(), qstrTemp.size(), 1, fp);
            if (fp != NULL)  fclose(fp);
        }
    }
}

void ImageProcAlgo::Init_TWDW()
{
    /// 天文定位-帧初始化设置
    unsigned int uiImageWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    double dblRD        =57.29578;
    double dblPixelSize =10;//像素大小：微米
    double dblXS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素横向对应的比例尺：角秒
    double dblYS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素纵向对应的比例尺：角秒
    double dbljing      =m_pGParam->m_SObsParams.fLongitude/dblRD;
    double dblwei       =m_pGParam->m_SObsParams.fLatitude/dblRD;
    double dblgao       =m_pGParam->m_SObsParams.fAltitude;
    int    nSTARLIBX    =uiImageWidth/2;
    int    nSTARLIBY    =uiImageHeight/2;
    int    nCenter_IMGX =uiImageWidth/2;
    int    nCenter_IMGY =uiImageHeight/2;//实际图像大小
    int    nSHBH        =m_pGParam->m_SObsParams.iObsID;//设备站号
    int    nZBX         =0;//坐标系：0为地平，1为水平，2，为时角，3为赤道
    int    nModelCoefs  =7;//模型系数个数3，6，7；
    bool   bSaveData    =true;//输出恒星匹配信息、天文定位内符合精度，测光内符合精度，
    QString strAppPath  ="/home/dps/temp";
    char  AppPath[400];
    strcpy(AppPath,strAppPath.toLocal8Bit().data());
    qDebug()<<strAppPath;
    strAppPath=AppPath;
    QDir dir;
    QString strDir=AppPath;
    dir.mkdir(strDir);
    strDir+="/NFH";
    dir.mkdir(strDir);
    strDir=AppPath;
    strDir+="/PAS";
    dir.mkdir(strDir);
    m_pstarmap->Calc_Paras_Initial(dblPixelSize,dblXS,dblYS,
                                dbljing,dblwei,dblgao,
                                nSTARLIBX,nSTARLIBY,
                                nCenter_IMGX,nCenter_IMGY);
    m_pstarmap->dblMinMagMX=13.0;
//    m_pstarmap->Star_SetImage_XX_DelXY(-m_pGParam->m_SOpticData.fRotateDeg,0,0);
    m_pstarmap->SetStarMatched(48);
    m_pstarmap->bSaveStarNFH = true;
    QString qstrNFH = AppPath;
    qstrNFH += "/NFH";
    m_pstarmap->Calc_DATA_ZH_Parameter(m_pGParam->m_SObsParams.iObsID,//测站代号：四位整数
                           0,//坐标系类型，：0为地平坐标系，1为水平坐标系，2为时角坐标系，3为赤道坐标系
                           3,//模型系数的个数：3，6，7；
                           true,//中间结果输出标志
                           qstrNFH.toLocal8Bit().data(),//中间结果输出文件的目录
                           "gtw11111",//中间结果输出文件名
                            qstrNFH.toLocal8Bit().data(),//天文定位资料保存目录
                            true);//输入数据的标志，缺省为：真
}

void ImageProcAlgo::InitPhotometry(void)
{
    unsigned int uiImageWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;

    std::string strCatalog = "/home/dps/IniFiles/catalog/UCAC/";

    if (m_pPhotometry == nullptr)
    {
        TCatalog sTCatalog(strCatalog);
        TSitePos sTSitePos(m_pGParam->m_SObsParams.fLongitude, m_pGParam->m_SObsParams.fLatitude, m_pGParam->m_SObsParams.fAltitude);
        TWeather sTWeather(m_pGParam->m_sNetAtmosData.fTemp, m_pGParam->m_sNetAtmosData.fHumidity, m_pGParam->m_sNetAtmosData.fAtmosP/100.0);
        Toptic sToptic(1350, 1200, uiImageWidth * m_pGParam->m_SOpticData.fPixelScale * 60.0, 10, 10, uiImageWidth, uiImageHeight,
                                uiImageWidth * m_pGParam->m_SOpticData.fPixelScale * 60.0, uiImageHeight * m_pGParam->m_SOpticData.fPixelScale * 60.0,
                                m_pGParam->m_SOpticData.fPixelScale * 3600.0, m_pGParam->m_SOpticData.fPixelScale * 3600.0);
        m_pPhotometry = new DllPhotometry(sTCatalog, sTSitePos, sTWeather, sToptic);
        m_pPhotometry->setCataInfo(strCatalog);
    }
    else
    {
        Toptic sToptic(1350, 1200, uiImageWidth * m_pGParam->m_SOpticData.fPixelScale * 60.0, 10, 10, uiImageWidth, uiImageHeight,
                            uiImageWidth * m_pGParam->m_SOpticData.fPixelScale * 60.0, uiImageHeight * m_pGParam->m_SOpticData.fPixelScale * 60.0,
                            m_pGParam->m_SOpticData.fPixelScale * 3600.0, m_pGParam->m_SOpticData.fPixelScale * 3600.0);
        m_pPhotometry->setOpticInfo(sToptic);
    }
}

void ImageProcAlgo::SetPhotometry(void)
{
    unsigned int uiImageWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;

    DateTime dt;
    unsigned short* pausFlat = NULL;
    unsigned short* pausBias = NULL;
    pausFlat = GetFlatImage(uiImageWidth, uiImageHeight);
    pausBias = GetBiasImage(uiImageWidth, uiImageHeight);

    if (pausBias)   // Bias must be before Flat
    {
        ImgData infoBias;
        infoBias.setData(pausBias, uiImageWidth, uiImageHeight,
                     dt, 1.0, 0, 0, "N");
        std::vector<ImgData> vectInfoBias;
        vectInfoBias.push_back(infoBias);
        m_pPhotometry->BiasProcess(vectInfoBias);
        int a = 0;
        a++;
    }

    if (pausFlat)
    {
        ImgData infoFlat;
        infoFlat.setData(pausFlat, uiImageWidth, uiImageHeight,
                     dt, 1.0, 0, 0, "N");
        std::vector<ImgData> vectInfoFlat;
        vectInfoFlat.push_back(infoFlat);
        m_pPhotometry->FlatProcess(vectInfoFlat);
    }

    m_pPhotometry->m_distorcoef[0] = m_dblJB_XCOEF[0];
    m_pPhotometry->m_distorcoef[1] = m_dblJB_XCOEF[1];
    m_pPhotometry->m_distorcoef[2] = m_dblJB_XCOEF[2];
    m_pPhotometry->m_distorcoef[3] = m_dblJB_XCOEF[3];
    m_pPhotometry->m_distorcoef[4] = m_dblJB_XCOEF[4];
    m_pPhotometry->m_distorcoef[5] = m_dblJB_XCOEF[5];
    m_pPhotometry->m_distorcoef[6+0] = m_dblJB_YCOEF[0];
    m_pPhotometry->m_distorcoef[6+1] = m_dblJB_YCOEF[1];
    m_pPhotometry->m_distorcoef[6+2] = m_dblJB_YCOEF[2];
    m_pPhotometry->m_distorcoef[6+3] = m_dblJB_YCOEF[3];
    m_pPhotometry->m_distorcoef[6+4] = m_dblJB_YCOEF[4];
    m_pPhotometry->m_distorcoef[6+5] = m_dblJB_YCOEF[5];
}

void ImageProcAlgo::ChangeGTWStr(char *pacOldStr, char *pacNewStr, char* pacExpStr)
{
    QString qstrOldStr(pacOldStr);
    QStringList qstrlistOldStr = qstrOldStr.split(' ');
    qstrlistOldStr[3] = "2106";

    int iTrackMode = m_iTrackMode;
    int iProcMode = m_iProcMode;

    if (iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO)
        qstrlistOldStr[18] = "150";
    else if (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
        qstrlistOldStr[18] = "200";
    else if (iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
        qstrlistOldStr[18] = "300";
    else
        qstrlistOldStr[18] = "150";

    if (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
        qstrlistOldStr[19] = "0";
    else
        qstrlistOldStr[19] = "1";

    if (qstrlistOldStr[20] == "0250")
        qstrlistOldStr[20] = "0999";

    QString qstrNewStr = "";
    for (int i = 0; i < qstrlistOldStr.size(); i++)
    {
        if (qstrlistOldStr[i] == " ")
            qstrNewStr += " ";
        else
        {
            qstrNewStr += qstrlistOldStr[i];
            qstrNewStr += " ";
        }
    }
    memcpy(pacNewStr, qstrNewStr.toLocal8Bit().data(), qstrNewStr.size());
    memcpy(pacExpStr, qstrlistOldStr[11].toLocal8Bit().data(), qstrlistOldStr[11].size());
}

void ImageProcAlgo::ChangeGDJStr(char *pacOldStr, char *pacNewStr)
{
    QString qstrOldStr(pacOldStr);
    QStringList qstrlistOldStr = qstrOldStr.split(' ');
    qstrlistOldStr[3] = "2106";

    int iTrackMode = m_iTrackMode;
    int iProcMode = m_iProcMode;

    if (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
        qstrlistOldStr[15] = "0";
    else
        qstrlistOldStr[15] = "1";

    if (qstrlistOldStr[12] == "02500")
        qstrlistOldStr[12] = "09990";

    QString qstrNewStr = "";
    for (int i = 0; i < qstrlistOldStr.size(); i++)
    {
        if (qstrlistOldStr[i] == " ")
            qstrNewStr += " ";
        else
        {
            qstrNewStr += qstrlistOldStr[i];
            qstrNewStr += " ";
        }
    }
    memcpy(pacNewStr, qstrNewStr.toLocal8Bit().data(), qstrNewStr.size());
}

void ImageProcAlgo::WriteFile(QString qstrPathName, QString qstrFileName, QString qstrTaskID, QString qstrObsID, QString qstrStartTime, QString qstrEndTime, char* paucInfo)
{
    QString qstrTotalName = qstrPathName + "/" + qstrFileName;

    /// 判断文件是否为空
    bool bEmpty;
    {
        vector<string> tmp_files;
        ifstream infile(qstrTotalName.toLocal8Bit().data());
        string lineContent;
        while ( getline( infile, lineContent, '\n' ) )
        {
            tmp_files.push_back(lineContent + "\n" );
        }
        infile.close();
        bEmpty = tmp_files.size() == 0;
    }

    fstream file;
    if(bEmpty)  // 文件为空，添加文件头
    {
        file.open(qstrTotalName.toLocal8Bit().data(),ios::app);
        file << "C " << qstrFileName.toLocal8Bit().data() << "\r\n";
        file << "C " << qstrTaskID.toLocal8Bit().data() << "\r\n";
        file << "C " << qstrStartTime.toLocal8Bit().data() << "\r\n";
        file << "C " << qstrEndTime.toLocal8Bit().data() << "\r\n";
        file << "C "  << "\r\n";
        file << "C " << qstrObsID.toLocal8Bit().data() << "\r\n";
        file << "C " << "\r\n";
        file << "C " << "\r\n";
        file << "C " << "\r\n";
        file << "C " << "\r\n";
        file.close();
    }
    else    // 文件不为空
    {
        vector<string> tmp_files;
        ifstream infile(qstrTotalName.toLocal8Bit().data());
        string lineContent;
        while ( getline( infile, lineContent, '\n' ) )
        {
            tmp_files.push_back(lineContent + "\n" );
        }
        infile.close();

        /// 修改任务结束时间
        tmp_files[3] = "C " + qstrEndTime.toStdString() + "\r\n";

        /// 删除最后一行的“C END\n”
        ofstream outfile(qstrTotalName.toLocal8Bit().data(), ios::trunc);
        copy(tmp_files.begin(), tmp_files.end()-1, ostream_iterator<string>(outfile));
        outfile.close();
    }

    /// 添加数据内容与“END\n”
    file.open(qstrTotalName.toLocal8Bit().data(),ios::app);
    file << paucInfo << endl;
    file << "C END" << endl;
    file.close();
}

void ImageProcAlgo::WritePasFile(QString qstrPathName, QString qstrFileName, char* pacInfo)
{
    QString qstrTotalName = qstrPathName + "/" + qstrFileName;

    fstream file;
    file.open(qstrTotalName.toLocal8Bit().data(),ios::app);
    file << pacInfo << endl;
    file.close();
}

void ImageProcAlgo::ChangeGAEStrMv(char *pacOldStr, char *pacNewStr, float fMv)
{
    QString qstrOldStr(pacOldStr);
    QStringList qstrlistOldStr = qstrOldStr.split(' ');
    if (fMv >= 0)
        qstrlistOldStr[20].sprintf("0%03d", (int)floor(fMv*10));
    else
        qstrlistOldStr[20].sprintf("1%03d", (int)floor(abs(fMv*10)));
    QString qstrNewStr = "";
    for (int i = 0; i < qstrlistOldStr.size(); i++)
    {
        if (qstrlistOldStr[i] == " ")
            qstrNewStr += " ";
        else
        {
            qstrNewStr += qstrlistOldStr[i];
            qstrNewStr += " ";
        }
    }
    memcpy(pacNewStr, qstrNewStr.toLocal8Bit().data(), qstrNewStr.size());
}

void ImageProcAlgo::TWDataDeal(unsigned long long ullFrameID)
{
    unsigned int uiImageWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    vector<sFramePacket>::iterator iter = m_vectFramePacket.begin();
    for(; iter != m_vectFramePacket.end(); iter++)
    {
        if(ullFrameID == (*iter).ulFrameSeq)
            break;
    }
    if(iter == m_vectFramePacket.end())
        return;
    /// 天文定位数据整形
    double dJBDx, dJBDy;
    dJBDx = 0; dJBDy = 0;
    int nYear, nMonth, nDay, nHour, nMinute, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
    double dblSecond, dblSecondNew, dSecondAdd, dblHour, dblHourNew, dblPointA, dblPointE, dblwendu, dbldqy, dblshidu;
    BJ2UTC((*iter).stimeFrame.iYear,
            (*iter).stimeFrame.iMonth,
            (*iter).stimeFrame.iDay,
            (*iter).stimeFrame.iHour,
            nYear, nMonth, nDay, nHour);
    nMinute = (*iter).stimeFrame.iMinute;
    dblSecond = (*iter).stimeFrame.iSecond + (*iter).stimeFrame.iMillisecond * 0.001 + (*iter).stimeFrame.iMicrosecond * 0.000001;
    dblHour = nHour + nMinute/60e0 + dblSecond/3600e0;
    dblPointA = (*iter).pairfFOVCenterAEModify.first;
    dblPointE = (*iter).pairfFOVCenterAEModify.second;
    dblwendu = (*iter).fTemp+273.15;
    dbldqy = (*iter).fAtmosP/100.0;
    dblshidu = (*iter).fHumidity;

    (*iter).stwdwFixData.nYear = nYear;
    (*iter).stwdwFixData.nMonth = nMonth;
    (*iter).stwdwFixData.nDay = nDay;
    (*iter).stwdwFixData.nHour = nHour;
    (*iter).stwdwFixData.nMinute = nMinute;
    (*iter).stwdwFixData.dblHour = dblHour;
    (*iter).stwdwFixData.dblPointA = dblPointA;
    (*iter).stwdwFixData.dblPointE = dblPointE;
    (*iter).stwdwFixData.dblwendu = dblwendu;
    (*iter).stwdwFixData.dbldqy = dbldqy;
    (*iter).stwdwFixData.dblshidu = dblshidu;
    (*iter).stwdwFixData.dblSecond = dblSecond;

    int             nStarShow=0;
    int             nStarCrop=0;
    for (int i = 0; i < (*iter).vectStarMeasures.size(); i++)
    {
        /// Select Star
        if ((*iter).vectStarMeasures[i].fArea > 15
                && (*iter).vectStarMeasures[i].pairfPos.first >= m_pGParam->m_SOpticData.fOptCenterX - uiImageWidth*7/16
                && (*iter).vectStarMeasures[i].pairfPos.first <= m_pGParam->m_SOpticData.fOptCenterX + uiImageWidth*7/16
                && (*iter).vectStarMeasures[i].pairfPos.second >= m_pGParam->m_SOpticData.fOptCenterY - uiImageHeight*7/16
                && (*iter).vectStarMeasures[i].pairfPos.second <= m_pGParam->m_SOpticData.fOptCenterY + uiImageHeight*7/16)
        {
            (*iter).stwdwFixData.dblStarShow[nStarShow][0] = (*iter).vectStarMeasures[i].pairfPos.first;
            (*iter).stwdwFixData.dblStarShow[nStarShow][1] = (*iter).vectStarMeasures[i].pairfPos.second;
            (*iter).stwdwFixData.dblStarShow[nStarShow][2]=(uint)((*iter).vectStarMeasures[i].fArea);//像素数
            (*iter).stwdwFixData.dblStarShow[nStarShow][3]=(uint)((*iter).vectStarMeasures[i].fArea);//灰度值
            nStarShow++;
        }
        if (!m_pGParam->m_SImageProcessorData.bFullLEO || m_pGParam->m_SDataProcessorData.bForceRePoint)
        {
            if ((*iter).vectStarMeasures[i].fArea > 15
                    && (*iter).vectStarMeasures[i].pairfPos.first >= m_pGParam->m_SOpticData.fOptCenterX - uiImageWidth*4/16
                    && (*iter).vectStarMeasures[i].pairfPos.first <= m_pGParam->m_SOpticData.fOptCenterX + uiImageWidth*4/16
                    && (*iter).vectStarMeasures[i].pairfPos.second >= m_pGParam->m_SOpticData.fOptCenterY - uiImageHeight*4/16
                    && (*iter).vectStarMeasures[i].pairfPos.second <= m_pGParam->m_SOpticData.fOptCenterY + uiImageHeight*4/16)
            {
                (*iter).stwdwFixData.dblStarCrop[nStarCrop][0] = (*iter).vectStarMeasures[i].pairfPos.first;
                (*iter).stwdwFixData.dblStarCrop[nStarCrop][1] = (*iter).vectStarMeasures[i].pairfPos.second;
                (*iter).stwdwFixData.dblStarCrop[nStarCrop][2]=(uint)((*iter).vectStarMeasures[i].fArea);//像素数
                (*iter).stwdwFixData.dblStarCrop[nStarCrop][3]=(uint)((*iter).vectStarMeasures[i].fArea);//灰度值
                nStarCrop++;
            }
        }
    }
    /// Sort Star
    nStarShow = nStarShow > 3000 ? 3000 : nStarShow;
    {
        float tmp0 = (*iter).stwdwFixData.dblStarShow[0][0];
        float tmp1 = (*iter).stwdwFixData.dblStarShow[0][1];
        float tmp2 = (*iter).stwdwFixData.dblStarShow[0][2];
        float tmp3 = (*iter).stwdwFixData.dblStarShow[0][3];
        for (int i = 0; i < nStarShow-1; i++)
        {
            for (int j = 1; j < nStarShow; j++)
            {
                if ((*iter).stwdwFixData.dblStarShow[i][2] > (*iter).stwdwFixData.dblStarShow[j][2])
                {
                    tmp0 = (*iter).stwdwFixData.dblStarShow[i][0];
                    tmp1 = (*iter).stwdwFixData.dblStarShow[i][1];
                    tmp2 = (*iter).stwdwFixData.dblStarShow[i][2];
                    tmp3 = (*iter).stwdwFixData.dblStarShow[i][3];
                    (*iter).stwdwFixData.dblStarShow[i][0] = (*iter).stwdwFixData.dblStarShow[j][0];
                    (*iter).stwdwFixData.dblStarShow[i][1] = (*iter).stwdwFixData.dblStarShow[j][1];
                    (*iter).stwdwFixData.dblStarShow[i][2] = (*iter).stwdwFixData.dblStarShow[j][2];
                    (*iter).stwdwFixData.dblStarShow[i][3] = (*iter).stwdwFixData.dblStarShow[j][3];
                    (*iter).stwdwFixData.dblStarShow[j][0] = tmp0;
                    (*iter).stwdwFixData.dblStarShow[j][1] = tmp1;
                    (*iter).stwdwFixData.dblStarShow[j][2] = tmp2;
                    (*iter).stwdwFixData.dblStarShow[j][3] = tmp3;
                }
            }
        }
    }
    if (!m_pGParam->m_SImageProcessorData.bFullLEO || m_pGParam->m_SDataProcessorData.bForceRePoint)
    {
        nStarCrop = nStarCrop > 2000 ? 2000 : nStarCrop;
        {
            float tmp0 = (*iter).stwdwFixData.dblStarCrop[0][0];
            float tmp1 = (*iter).stwdwFixData.dblStarCrop[0][1];
            float tmp2 = (*iter).stwdwFixData.dblStarCrop[0][2];
            float tmp3 = (*iter).stwdwFixData.dblStarCrop[0][3];
            for (int i = 0; i < nStarCrop-1; i++)
            {
                for (int j = 1; j < nStarCrop; j++)
                {
                    if ((*iter).stwdwFixData.dblStarCrop[i][2] > (*iter).stwdwFixData.dblStarCrop[j][2])
                    {
                        tmp0 = (*iter).stwdwFixData.dblStarCrop[i][0];
                        tmp1 = (*iter).stwdwFixData.dblStarCrop[i][1];
                        tmp2 = (*iter).stwdwFixData.dblStarCrop[i][2];
                        tmp3 = (*iter).stwdwFixData.dblStarCrop[i][3];
                        (*iter).stwdwFixData.dblStarCrop[i][0] = (*iter).stwdwFixData.dblStarCrop[j][0];
                        (*iter).stwdwFixData.dblStarCrop[i][1] = (*iter).stwdwFixData.dblStarCrop[j][1];
                        (*iter).stwdwFixData.dblStarCrop[i][2] = (*iter).stwdwFixData.dblStarCrop[j][2];
                        (*iter).stwdwFixData.dblStarCrop[i][3] = (*iter).stwdwFixData.dblStarCrop[j][3];
                        (*iter).stwdwFixData.dblStarCrop[j][0] = tmp0;
                        (*iter).stwdwFixData.dblStarCrop[j][1] = tmp1;
                        (*iter).stwdwFixData.dblStarCrop[j][2] = tmp2;
                        (*iter).stwdwFixData.dblStarCrop[j][3] = tmp3;
                    }
                }
            }
        }
    }
    (*iter).stwdwFixData.bisTrue = true;
    (*iter).stwdwFixData.nStarShow = nStarShow;
    (*iter).stwdwFixData.nStarCrop = nStarCrop;

    //*************************** Calculate AE Point Error ***************************//
    if (!m_pGParam->m_SImageProcessorData.bFullLEO || m_pGParam->m_SDataProcessorData.bForceRePoint)
    {
        double  dblPT_P[3]={0,0,0};
        int nStarMatched = 0, iNumStarMatched = 0;
        double  dblCalcPointErrorsResult[5];
        m_pstarmap->Star_SetImage_XX_DelXY(0, 0, 0);
        m_pstarmap->CalcPoint_Errors_WithTimePointSortNew((*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay, (*iter).stwdwFixData.dblHour, (*iter).stwdwFixData.dblPointA, (*iter).stwdwFixData.dblPointE,
                                                       (*iter).stwdwFixData.dblwendu,(*iter).stwdwFixData.dbldqy,(*iter).stwdwFixData.dblshidu,
                                                       dblPT_P, &(*iter).stwdwFixData.dblStarCrop[0][0], (*iter).stwdwFixData.nStarCrop > 100 ? 100 : (*iter).stwdwFixData.nStarCrop, &nStarMatched,
                                                       dblCalcPointErrorsResult, false);

        if (nStarMatched > 6)
        {
            (*iter).stwdwFixData.qPairMatchErrorRes.first = nStarMatched;
            (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrAzi", dblCalcPointErrorsResult[3]);
            (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrEle", dblCalcPointErrorsResult[4]);
            (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fRotate", dblCalcPointErrorsResult[2]);
            iNumStarMatched = nStarMatched;
        }
        else
        {
            if(iter != m_vectFramePacket.begin())
            {
                if((*(iter - 1)).stwdwFixData.qPairMatchErrorRes.first)
                {
                    (*iter).stwdwFixData.qPairMatchErrorRes.first = (*(iter - 1)).stwdwFixData.qPairMatchErrorRes.first;
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrAzi", (*(iter - 1)).stwdwFixData.qPairMatchErrorRes.second.value("fErrAzi"));
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrEle", (*(iter - 1)).stwdwFixData.qPairMatchErrorRes.second.value("fErrEle"));
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fRotate", (*(iter - 1)).stwdwFixData.qPairMatchErrorRes.second.value("fRotate"));
                    iNumStarMatched = (*(iter - 1)).stwdwFixData.qPairMatchErrorRes.first;
                }
                else
                {
                    (*iter).stwdwFixData.qPairMatchErrorRes.first = 0;
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrAzi", 0);
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrEle", 0);
                    (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fRotate", 0);
                    iNumStarMatched = 0;
                }
            }
            else
            {
                (*iter).stwdwFixData.qPairMatchErrorRes.first = 0;
                (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrAzi", 0);
                (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fErrEle", 0);
                (*iter).stwdwFixData.qPairMatchErrorRes.second.insert("fRotate", 0);
                iNumStarMatched = 0;
            }
        }
        m_pGParam->m_SDataProcessorData.fErrAzi = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fErrAzi");
        m_pGParam->m_SDataProcessorData.fErrEle = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fErrEle");
        m_pGParam->m_SDataProcessorData.fRotate = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fRotate");
        m_pGParam->m_SDataProcessorData.iNumStarMatched = iNumStarMatched;
        m_pGParam->m_SDataProcessorData.bValid = nStarMatched > 6 ? true : false;
        m_pGParam->m_SDataProcessorData.bReady = true;
    }
}

void ImageProcAlgo::ProcessTWDW_GDCL(vector<int> vecTarget, unsigned long long ullFrameID)
{
    unsigned int uiObsID = m_pGParam->m_SImageProcessorData.bProcessMode ? m_pGParam->m_SObsParams.iObsID : m_pGParam->m_SImageReplayerData.qstrTeleID.toUInt();
    unsigned int uiImageWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    vector<sFramePacket>::iterator iter = m_vectFramePacket.begin();
    for(; iter != m_vectFramePacket.end(); iter++)
    {
        if(ullFrameID == (*iter).ulFrameSeq)
            break;
    }
    if(iter == m_vectFramePacket.end())
        return;


    pair<double, double> pairdRefCenter(m_pGParam->m_SOpticData.fOptCenterX, m_pGParam->m_SOpticData.fOptCenterY);
    pair<double, double> pairdRefUp(m_pGParam->m_SOpticData.fOptCenterX, m_pGParam->m_SOpticData.fOptCenterY - uiImageHeight / 4);
    pair<double, double> pairdRefDown(m_pGParam->m_SOpticData.fOptCenterX, m_pGParam->m_SOpticData.fOptCenterY + uiImageHeight / 4);
    pair<double, double> pairdRefUp2(m_pGParam->m_SOpticData.fOptCenterX, m_pGParam->m_SOpticData.fOptCenterY - uiImageHeight * 3 / 8);
    pair<double, double> pairdRefDown2(m_pGParam->m_SOpticData.fOptCenterX, m_pGParam->m_SOpticData.fOptCenterY + uiImageHeight * 3 / 8);
    pair<double, double> pairdRefLeft1(m_pGParam->m_SOpticData.fOptCenterX - uiImageWidth / 4, m_pGParam->m_SOpticData.fOptCenterY);
    pair<double, double> pairdRefLeft2(m_pGParam->m_SOpticData.fOptCenterX - uiImageWidth * 3 / 8, m_pGParam->m_SOpticData.fOptCenterY);
    pair<double, double> pairdRefRight1(m_pGParam->m_SOpticData.fOptCenterX + uiImageWidth / 4, m_pGParam->m_SOpticData.fOptCenterY);
    pair<double, double> pairdRefRight2(m_pGParam->m_SOpticData.fOptCenterX + uiImageWidth * 3 / 8, m_pGParam->m_SOpticData.fOptCenterY);

    double          (*dblSateShow)[9];
    dblSateShow = new double[2002][9];
    double          (*dblSateAE)[9];
    dblSateAE = new double[2002][9];
    int             nSateShow=0;
    for (int iNum = 0; iNum < vecTarget.size(); iNum++)
    {
        int i = vecTarget[iNum];
        int curIndex = iter != (m_vectFramePacket.end() - 1) ? 3 - ((*(m_vectFramePacket.end() - 1)).ulFrameSeq - ullFrameID) : m_vectTargetInfo[i].vectResPacket.size() - 1;
        dblSateShow[nSateShow][0] = m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.first;
        dblSateShow[nSateShow][1] = m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.second;
        dblSateAE[nSateShow][0] = m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosZXDW.first;
        dblSateAE[nSateShow][1] = m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosZXDW.second;
        nSateShow++;
    }

    double dJBDx = 0.0, dJBDy = 0.0;
    dblSateShow[nSateShow][0] = pairdRefCenter.first;
    dblSateShow[nSateShow][1] = pairdRefCenter.second;
    CalcDistortionDelta(dblSateShow[nSateShow][0], dblSateShow[nSateShow][1], dJBDx, dJBDy);
    dblSateShow[nSateShow][0] -= dJBDx;
    dblSateShow[nSateShow][1] -= dJBDy;

    dblSateShow[nSateShow+1][0] = pairdRefUp.first;
    dblSateShow[nSateShow+1][1] = pairdRefUp.second;
    CalcDistortionDelta(dblSateShow[nSateShow+1][0], dblSateShow[nSateShow+1][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+1][0] -= dJBDx;
    dblSateShow[nSateShow+1][1] -= dJBDy;

    dblSateShow[nSateShow+2][0] = pairdRefUp2.first;
    dblSateShow[nSateShow+2][1] = pairdRefUp2.second;
    CalcDistortionDelta(dblSateShow[nSateShow+2][0], dblSateShow[nSateShow+2][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+2][0] -= dJBDx;
    dblSateShow[nSateShow+2][1] -= dJBDy;

    dblSateShow[nSateShow+3][0] = pairdRefDown.first;
    dblSateShow[nSateShow+3][1] = pairdRefDown.second;
    CalcDistortionDelta(dblSateShow[nSateShow+3][0], dblSateShow[nSateShow+3][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+3][0] -= dJBDx;
    dblSateShow[nSateShow+3][1] -= dJBDy;

    dblSateShow[nSateShow+4][0] = pairdRefDown2.first;
    dblSateShow[nSateShow+4][1] = pairdRefDown2.second;
    CalcDistortionDelta(dblSateShow[nSateShow+4][0], dblSateShow[nSateShow+4][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+4][0] -= dJBDx;
    dblSateShow[nSateShow+4][1] -= dJBDy;

    dblSateShow[nSateShow+5][0] = pairdRefLeft1.first;
    dblSateShow[nSateShow+5][1] = pairdRefLeft1.second;
    CalcDistortionDelta(dblSateShow[nSateShow+5][0], dblSateShow[nSateShow+5][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+5][0] -= dJBDx;
    dblSateShow[nSateShow+5][1] -= dJBDy;

    dblSateShow[nSateShow+6][0] = pairdRefLeft2.first;
    dblSateShow[nSateShow+6][1] = pairdRefLeft2.second;
    CalcDistortionDelta(dblSateShow[nSateShow+6][0], dblSateShow[nSateShow+6][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+6][0] -= dJBDx;
    dblSateShow[nSateShow+6][1] -= dJBDy;

    dblSateShow[nSateShow+7][0] = pairdRefRight1.first;
    dblSateShow[nSateShow+7][1] = pairdRefRight1.second;
    CalcDistortionDelta(dblSateShow[nSateShow+7][0], dblSateShow[nSateShow+7][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+7][0] -= dJBDx;
    dblSateShow[nSateShow+7][1] -= dJBDy;

    dblSateShow[nSateShow+8][0] = pairdRefRight2.first;
    dblSateShow[nSateShow+8][1] = pairdRefRight2.second;
    CalcDistortionDelta(dblSateShow[nSateShow+8][0], dblSateShow[nSateShow+8][1], dJBDx, dJBDy);
    dblSateShow[nSateShow+8][0] -= dJBDx;
    dblSateShow[nSateShow+8][1] -= dJBDy;

    int nStarsCalc = 0;
    double dblSatelliteAlpha[500], dblSatelliteDelta[500], dblSatelliteMag[500];//若大数组建议用new
    memset(dblSatelliteAlpha,0,4000);
    memset(dblSatelliteDelta,0,4000);
    memset(dblSatelliteMag  ,0,4000);

    /// 天文定位計算
    QString strExpTime;
    strExpTime.sprintf("%04d-%02d-%02d %d:%02d:%02d.%03d",
                       (*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay, (*iter).stwdwFixData.nHour,
                       (*iter).stimeFrame.iMinute,
                       (*iter).stimeFrame.iSecond,
                       (*iter).stimeFrame.iMillisecond);
    qDebug() << "* Exp Time:" << strExpTime;

    if (m_pGParam->m_SImageProcessorData.bFullLEO)  // Full LEO Binning
    {
        double dblRD        =57.29578;
        double dblPixelSize =10;//像素大小：微米
        double dblXS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素横向对应的比例尺：角秒
        double dblYS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素纵向对应的比例尺：角秒
        double dbljing      =m_pGParam->m_SObsParams.fLongitude/dblRD;
        double dblwei       =m_pGParam->m_SObsParams.fLatitude/dblRD;
        double dblgao       =m_pGParam->m_SObsParams.fAltitude;
        int    nSTARLIBX    =uiImageWidth/2;
        int    nSTARLIBY    =uiImageHeight/2;
        int    nCenter_IMGX =uiImageWidth/2;
        int    nCenter_IMGY =uiImageHeight/2;//实际图像大小
        m_pstarmap->Calc_Paras_Initial(dblPixelSize*2,dblXS*2,dblYS*2,
                                    dbljing,dblwei,dblgao,
                                    nSTARLIBX/2,nSTARLIBY/2,
                                    nCenter_IMGX/2,nCenter_IMGY/2);
        m_pstarmap->SetStarMatched(36);
        for (int i = 0; i < (*iter).stwdwFixData.nStarShow; i++)
        {
            (*iter).stwdwFixData.dblStarShow[i][0] *= 0.5;
            (*iter).stwdwFixData.dblStarShow[i][1] *= 0.5;
        }
        for (int i = 0; i < nSateShow+9; i++)
        {
            dblSateShow[i][0] *= 0.5;
            dblSateShow[i][1] *= 0.5;
        }
        for (int i = 0; i < (*iter).stwdwFixData.nStarCrop; i++)
        {
            (*iter).stwdwFixData.dblStarCrop[i][0] *= 0.5;
            (*iter).stwdwFixData.dblStarCrop[i][1] *= 0.5;
        }
    }
    else
    {
        double dblRD        =57.29578;
        double dblPixelSize =10;//像素大小：微米
        double dblXS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素横向对应的比例尺：角秒
        double dblYS        =m_pGParam->m_SOpticData.fPixelScale * 3600;//每个像素纵向对应的比例尺：角秒
        double dbljing      =m_pGParam->m_SObsParams.fLongitude/dblRD;
        double dblwei       =m_pGParam->m_SObsParams.fLatitude/dblRD;
        double dblgao       =m_pGParam->m_SObsParams.fAltitude;
        int    nSTARLIBX    =uiImageWidth/2;
        int    nSTARLIBY    =uiImageHeight/2;
        int    nCenter_IMGX =uiImageWidth/2;
        int    nCenter_IMGY =uiImageHeight/2;//实际图像大小
        m_pstarmap->Calc_Paras_Initial(dblPixelSize,dblXS,dblYS,
                                    dbljing,dblwei,dblgao,
                                    nSTARLIBX,nSTARLIBY,
                                    nCenter_IMGX,nCenter_IMGY);
        m_pstarmap->SetStarMatched(48);
    }
    if (!m_pGParam->m_SImageProcessorData.bFullLEO || m_pGParam->m_SDataProcessorData.bForceRePoint)
    {
        m_pstarmap->Star_SetImage_XX_DelXY(0, 0, 0);
        if((*iter).stwdwFixData.qPairMatchErrorRes.first > 6)
        {
            double fErrAzi = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fErrAzi");
            double fErrEle = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fErrEle");
            double fRotate = (*iter).stwdwFixData.qPairMatchErrorRes.second.value("fRotate");
            m_pstarmap->Star_SetImage_XX_DelXY(-fRotate, fErrAzi, fErrEle);
        }
    }

    nStarsCalc=m_pstarmap->Satellite_Telescope_DJ((*iter).stwdwFixData.nYear,(*iter).stwdwFixData.nMonth,(*iter).stwdwFixData.nDay,(*iter).stwdwFixData.dblHour,(*iter).stwdwFixData.dblPointA,(*iter).stwdwFixData.dblPointE,
                                               &(*iter).stwdwFixData.dblStarShow[0][0],(*iter).stwdwFixData.nStarShow > 500 ? 500 :(*iter).stwdwFixData.nStarShow,
                                               0.0,(*iter).stwdwFixData.dblwendu,(*iter).stwdwFixData.dbldqy,(*iter).stwdwFixData.dblshidu,nSateShow+9,&dblSateShow[0][0],
                                               dblSatelliteAlpha,dblSatelliteDelta,dblSatelliteMag);
    m_pGParam->m_SDataProcessorData.iNumStarCalc = nStarsCalc;
    if (m_pGParam->m_SImageProcessorData.bFullLEO)  // Full LEO Binning
    {
        for (int i = 0; i < (*iter).stwdwFixData.nStarShow; i++)
        {
            (*iter).stwdwFixData.dblStarShow[i][0] *= 2;
            (*iter).stwdwFixData.dblStarShow[i][1] *= 2;
        }
        for (int i = 0; i < nSateShow+9; i++)
        {
            dblSateShow[i][0] *= 2;
            dblSateShow[i][1] *= 2;
        }
    }

    qDebug() << "* Calc Stars:" << nStarsCalc << "; Input Stars:" << (*iter).stwdwFixData.nStarShow;

    /// 光度計算 + 數據賦值
    if (nStarsCalc >= 3)
    {
        /// 光度計算
        vector<PositionInfo> vectposInfo;   // Reference Position 9 Package
        for (int i = 0; i < 9; i++)
        {
            PositionInfo posInfo(i, dblSatelliteAlpha[nSateShow + i], dblSatelliteDelta[nSateShow + i], 2000, 0, 0, dblSateShow[nSateShow + i][0], dblSateShow[nSateShow + i][1]);
            vectposInfo.push_back(posInfo);
        }
        vector<CataInfo> vectcataInfo;   // Targets Package
        for (int i = 0; i < nSateShow; i++)
        {
            CataInfo cataInfo(0, dblSatelliteAlpha[i], dblSatelliteDelta[i], 2000, dblSateShow[i][0], dblSateShow[i][1], 99, "N");
            vectcataInfo.push_back(cataInfo);
        }
        DateTime dtImg((*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay, (*iter).stwdwFixData.nHour,
                       (*iter).stimeFrame.iMinute,
                       (*iter).stimeFrame.iSecond,
                       (*iter).stimeFrame.iMillisecond);
        float* pfImgPhotometry = (float*)((*iter).safImagePhotometry.pvoidBuffer);
        ImgData imgInfo;
        imgInfo.setData(pfImgPhotometry, uiImageWidth, uiImageHeight,
                        dtImg, (*iter).fExpTime/1000.0, (*iter).stwdwFixData.dblPointA, (*iter).stwdwFixData.dblPointE, "N");
        ImgPack imginfoPack;
        imginfoPack.imgInfo = &imgInfo;
        imginfoPack.refPosRaDecXY = vectposInfo;
        imginfoPack.objcatainfo = vectcataInfo;
        imginfoPack.siteInfo.setData(m_pGParam->m_SObsParams.fLongitude, m_pGParam->m_SObsParams.fLatitude, m_pGParam->m_SObsParams.fAltitude);
        imginfoPack.obsWeather.setData((*iter).fTemp, (*iter).fHumidity, (*iter).fAtmosP/100.0);
        bool bError = false;

        try
        {
            qDebug() << "ObjPhotometry begin.";
            m_pPhotometry->cali_fov_scale = 0.7;
            m_pPhotometry->ObjPhotometry(&imginfoPack, 2, 0, 1);
            qDebug() << "ObjPhotometry right.";
        }
        catch (exception e)
        {
            bError = true;
            qDebug() << "ObjPhotometry error.";
        }

        ImgInfoPack* ttt;
        ttt = &m_pPhotometry->m_objInfoPack;

        /// 数据赋值
        for (int iNum = 0; iNum < vecTarget.size(); iNum++)
        {
            int i = vecTarget[iNum];
            int curIndex = iter != (m_vectFramePacket.end() - 1) ? 3 - ((*(m_vectFramePacket.end() - 1)).ulFrameSeq - ullFrameID) : m_vectTargetInfo[i].vectResPacket.size() - 1;
            m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosTWDW = pair<float,float>(dblSatelliteAlpha[iNum], dblSatelliteDelta[iNum]);
            if (bError || !(m_pPhotometry->m_objInfoPack.objobsinfo[iNum].mag >= -10.0 && m_pPhotometry->m_objInfoPack.objobsinfo[iNum].mag <= 23.0))
            {
                m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL = 99.9;
                m_vectTargetInfo[i].vectResPacket[curIndex].blob.fDN = 0;
            }
            else
            {
                m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL = m_pPhotometry->m_objInfoPack.objobsinfo[iNum].mag;
                m_vectTargetInfo[i].vectResPacket[curIndex].blob.fDN = m_pPhotometry->m_objInfoPack.objobsinfo[iNum].flux * (*iter).fExpTime/1000.0;
            }

            /// 调试信息打印
            QString qstrAlphaDelta, qstrImagePos, qstrTargetMag;
            qstrAlphaDelta.sprintf("(%f,%f)",
                                   m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosTWDW.first,
                                   m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosTWDW.second);
            qstrImagePos.sprintf("(%f,%f)",
                                 m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.first,
                                 m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.second);
            qstrTargetMag.sprintf("%.3f mV", m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL);
            qDebug() << "*" << m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID
                     << "(Ascension,Declination):" << qstrAlphaDelta
                     << "(ImagePosX,ImagePosY):" << qstrImagePos
                     << "Mag:" << qstrTargetMag;

            /// 输出信息格式化
            char strDWData[300]="";
            char strDWDataNew[300]="";
            char strCGData[300]="";
            char strCGDataNew[300]="";
            char strExpTime[300] = "";
            double dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
            int nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
            double dblSecondNew;
            TimeAddSeconds((*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay, (*iter).stwdwFixData.nHour, (*iter).stwdwFixData.nMinute, (*iter).stwdwFixData.dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
            double dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
            m_pstarmap->TWDWDataNewFormat_DJ(m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,dblSatelliteAlpha[iNum],dblSatelliteDelta[iNum],
                                          (*iter).stwdwFixData.dblwendu-273.15,(*iter).stwdwFixData.dblshidu,(*iter).stwdwFixData.dbldqy,
                                          m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL,
                                          true,0,strDWData,&dblSateShow[iNum][0]);
            ChangeGTWStr(strDWData, strDWDataNew, strExpTime);
            m_pstarmap->GDJDataNewFormat(m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,dblSateAE[iNum][0],dblSateAE[iNum][1],
                                         (*iter).stwdwFixData.dblwendu-273.15,(*iter).stwdwFixData.dblshidu,(*iter).stwdwFixData.dbldqy,
                                         m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL,
                                         m_vectTargetInfo[i].vectResPacket[curIndex].blob.fDN,
                                         true, 0, strCGData);
            ChangeGDJStr(strCGData, strCGDataNew);

            QString qstrPASPrint;
            qstrPASPrint.sprintf("%04d %02d %02d %02d %02d %07.4f %09.5f %09.5f %08.3f %08.3f %03d %06.2f 1",
                              (*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay,
                              nHourNew, nMinuteNew, dblSecondNew,
                              dblSatelliteAlpha[iNum], dblSatelliteDelta[iNum],
                              m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.first,
                              m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.second,
                              nStarsCalc, m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL);
            QByteArray qbaPASPrint;
            qbaPASPrint.append(qstrPASPrint);
            char* pacPASInfo = qbaPASPrint.data();

            /// 存储路径
            QString qstrYYYY, qstrYYYYMM, qstrYYYYMMDD, qstrDatePath, qstrSub, qstrFileNameGTW, qstrFileNameGDJ, qstrFileNamePAS;
            if (m_pGParam->m_SImageProcessorData.bProcessMode)
            {
                if (m_SNetMasterControlData.qstrStartTime.size() >= 14)
                {
                    qstrYYYY = m_SNetMasterControlData.qstrStartTime.mid(0, 4);
                    qstrYYYYMM = m_SNetMasterControlData.qstrStartTime.mid(0, 6);
                    qstrYYYYMMDD = m_SNetMasterControlData.qstrStartTime.mid(0, 8);
                    qstrDatePath = qstrYYYY + "/"
                            + qstrYYYYMM + "/"
                            + qstrYYYYMMDD + "/";
                    qstrSub = qstrDatePath + m_SNetMasterControlData.qstrStartTime + "_" + m_SNetMasterControlData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
                    qstrFileNameGTW = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".GTW";
                    qstrFileNameGDJ = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".GDJ";
                    qstrFileNamePAS = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".PAS";
                }
            }
            else
            {
                if (m_pGParam->m_SImageReplayerData.qstrStartTime.size() >= 14)
                {
                    qstrYYYY = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 4);
                    qstrYYYYMM = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 6);
                    qstrYYYYMMDD = m_pGParam->m_SImageReplayerData.qstrStartTime.mid(0, 8);
                    qstrDatePath = "Replay/" + qstrYYYY + "/"
                            + qstrYYYYMM + "/"
                            + qstrYYYYMMDD + "/";
                    qstrSub = qstrDatePath + m_pGParam->m_SImageReplayerData.qstrStartTime + "_" + m_pGParam->m_SImageReplayerData.qstrTargetID + "_" + QString::number(uiObsID) + ".DATA";
                    qstrFileNameGTW = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".GTW";
                    qstrFileNameGDJ = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".GDJ";
                    qstrFileNamePAS = m_vectTargetInfo[i].qstrSaveStartTime + "_" + m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID + "_" + QString::number(uiObsID) + ".PAS";
                }
            }
            if (m_qstrStorePath != m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub)
            {
                m_qstrStorePath = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub;
            }
            QDir qDirMake;
            if (!qDirMake.exists(m_qstrStorePath + "/GTW"))
            {
                qDirMake.mkpath(m_qstrStorePath + "/GTW");
            }
            if (!qDirMake.exists(m_qstrStorePath + "/GDJ"))
            {
                qDirMake.mkpath(m_qstrStorePath + "/GDJ");
            }
            if (!qDirMake.exists(m_qstrStorePath + "/PAS"))
            {
                qDirMake.mkpath(m_qstrStorePath + "/PAS");
            }

            /// 信息输出
            QString qstrTaskID = m_vectTargetInfo[i].qstrTargetID;
            QString qstrEndTime;
            qstrEndTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                (*iter).stimeFrame.iYear,
                                (*iter).stimeFrame.iMonth,
                                (*iter).stimeFrame.iDay,
                                (*iter).stimeFrame.iHour,
                                (*iter).stimeFrame.iMinute,
                                (*iter).stimeFrame.iSecond);
            if(m_vectTargetInfo[i].vectInfoInFrame[curIndex].bValid)
            {
                WriteFile(m_qstrStorePath + "/GTW",
                          qstrFileNameGTW,
                          qstrTaskID,
                          QString::number(uiObsID),
                          m_vectTargetInfo[i].qstrSaveStartTime,
                          qstrEndTime,
                          strDWDataNew);
                WriteFile(m_qstrStorePath + "/GDJ",
                          qstrFileNameGDJ,
                          qstrTaskID,
                          QString::number(uiObsID),
                          m_vectTargetInfo[i].qstrSaveStartTime,
                          qstrEndTime,
                          strCGDataNew);
                WritePasFile(m_qstrStorePath + "/PAS",
                             qstrFileNamePAS,
                             pacPASInfo);
            }
            {
                for (vector<sPacketGAE>::iterator iter = m_vectpacketGAE.end()-1; iter >= m_vectpacketGAE.begin(); iter--)
                {
                    sPacketGAE packet = *iter;
                    if (strcmp(strExpTime, packet.strExpTime) == 0
                            && packet.qstrObsID == QString::number(m_pGParam->m_SObsParams.iObsID))
                    {
                        char strDWDataNew[300]="";
                        ChangeGAEStrMv(packet.strDWDataNew, strDWDataNew, m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL);
                        WriteFile(packet.qstrStorePath,
                                  packet.qstrFileNameGAE,
                                  packet.qstrTaskID,
                                  packet.qstrObsID,
                                  packet.qstrStartTime,
                                  packet.qstrEndTime,
                                  strDWDataNew);
                        m_vectpacketGAE.erase(iter);
                        break;
                    }
                }
            }

            if (m_pGParam->m_SImageProcessorData.bProcessMode)
            {
                if (m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID.toUInt() == m_SNetMasterControlData.qstrTargetID.toUInt())
                {
                    sGDCLData data;
                    QDate dateNow(nYearNew, nMonthNew, nDayNew);
                    int iDaysNow = dateNow.toJulianDay();
                    QDate date1970(1970, 1, 1);
                    int iDays1970 = date1970.toJulianDay();
                    int iJD1970 = iDaysNow - iDays1970;
                    data.lJMS1970 = (long)iJD1970 * 24 * 3600 * 100 + (long)(dblHourNew * 3600.0) * 100;
                    data.cMeasureStatus = m_SNetMasterControlData.bSearch ? 1 : 2;
                    data.iTargetID = m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID.toUInt();
                    data.cDataFlag = 1;
                    data.iDN = (int)m_vectTargetInfo[i].vectResPacket[curIndex].blob.fDN ;
                    data.iMv = (int)(m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL * 100);
                    data.lDist = 0;
                    data.iAlpha = (int)(m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosTWDW.first * 3600.0);
                    data.iDelta = (int)(m_vectTargetInfo[i].vectResPacket[curIndex].pairfTargetPosTWDW.second * 3600.0);
                    data.iMvRes = 1;

                    char* pacSendData = new char[sizeof(sGDCLData)];
                    memcpy(pacSendData, (char*)&data, sizeof(sGDCLData));

                    NetExchange* pNet = (NetExchange*)m_pGParam->m_SNetExchangeData.pvoidThis;
                    pNet->SendGDCL(pacSendData, sizeof(sGDCLData));
                }
            }
        }
    }
    else
    {
        for (int iNum = 0; iNum < vecTarget.size(); iNum++)
        {
            int i = vecTarget[iNum];
            int curIndex = iter != (m_vectFramePacket.end() - 1) ? 3 - ((*(m_vectFramePacket.end() - 1)).ulFrameSeq - ullFrameID) : m_vectTargetInfo[i].vectResPacket.size() - 1;
            /// 输出信息格式化
            char strDWData[300]="";
            char strDWDataNew[300]="";
            char strExpTime[300] = "";
            double dSecondAdd = (m_pGParam->m_SOpticData.fOptCenterX - m_vectTargetInfo[i].vectResPacket[curIndex].blob.pairfPos.first) * 138.11712 / 6144.0 * 0.001;
            int nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew;
            double dblSecondNew;
            TimeAddSeconds((*iter).stwdwFixData.nYear, (*iter).stwdwFixData.nMonth, (*iter).stwdwFixData.nDay, (*iter).stwdwFixData.nHour, (*iter).stwdwFixData.nMinute, (*iter).stwdwFixData.dblSecond, dSecondAdd, nYearNew, nMonthNew, nDayNew, nHourNew, nMinuteNew, dblSecondNew);
            double dblHourNew = nHourNew + nMinuteNew/60e0 + dblSecondNew/3600e0;
            m_pstarmap->TWDWDataNewFormat_DJ(m_vectTargetInfo[i].vectResPacket[curIndex].qstrTargetID.toUInt(),uiObsID,nYearNew,nMonthNew,nDayNew,dblHourNew,dblSatelliteAlpha[iNum],dblSatelliteDelta[iNum],
                                          (*iter).stwdwFixData.dblwendu-273.15,(*iter).stwdwFixData.dblshidu,(*iter).stwdwFixData.dbldqy,
                                          m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL,
                                          true,0,strDWData,&dblSateShow[iNum][0]);
            ChangeGTWStr(strDWData, strDWDataNew, strExpTime);

            {
                for (vector<sPacketGAE>::iterator iter = m_vectpacketGAE.end()-1; iter >= m_vectpacketGAE.begin(); iter--)
                {
                    sPacketGAE packet = *iter;
                    if (strcmp(strExpTime, packet.strExpTime) == 0
                            && packet.qstrObsID == QString::number(m_pGParam->m_SObsParams.iObsID))
                    {
                        char strDWDataNew[300]="";
                        ChangeGAEStrMv(packet.strDWDataNew, strDWDataNew, m_vectTargetInfo[i].vectResPacket[curIndex].fTargetMvGDCL);
                        WriteFile(packet.qstrStorePath,
                                  packet.qstrFileNameGAE,
                                  packet.qstrTaskID,
                                  packet.qstrObsID,
                                  packet.qstrStartTime,
                                  packet.qstrEndTime,
                                  strDWDataNew);
                        m_vectpacketGAE.erase(iter);
                        break;
                    }
                }
            }
        }
    }

}

void ImageProcAlgo::GetFirstResPackTime(const char *strSrc, QString &res)
{
    QString qstrOldStr(strSrc);
    QStringList qstrlistOldStr = qstrOldStr.split(' ');
    QString res1 = qstrlistOldStr[10];
    QString res2 = qstrlistOldStr[11].mid(0, 6);
    res = res1 + res2;
}

bool ImageProcAlgo::ReadPointError(QString qstrFileName)
{
    QFile dxCorrectFile(qstrFileName);
    if (dxCorrectFile.open(QIODevice::ReadOnly))
    {
        QDataStream outdx(&dxCorrectFile);
        outdx.readRawData((char*)&m_errresPonit, sizeof(ErrorResult));
        dxCorrectFile.close();
         qDebug() << "Valid error result file.";
    }
    else
    {
        qDebug() << "No valid error result file.";
    }
}

void ImageProcAlgo::ReadJBCoef(QString qstrFile, double *padJB_XCoef, double *padJB_YCoef)
{
    QSettings* qsetSystemInit = new QSettings(qstrFile, QSettings::IniFormat);
    if (qsetSystemInit)
    {
        /// [JB_XCoef]
        qsetSystemInit->beginGroup("JB_XCoef");
        if (qsetSystemInit->contains("JB_XCoef1")
            && qsetSystemInit->contains("JB_XCoef2")
            && qsetSystemInit->contains("JB_XCoef3")
            && qsetSystemInit->contains("JB_XCoef4")
            && qsetSystemInit->contains("JB_XCoef5")
            && qsetSystemInit->contains("JB_XCoef6"))
        {
            padJB_XCoef[0] = qsetSystemInit->value("JB_XCoef1").toDouble();
            padJB_XCoef[1] = qsetSystemInit->value("JB_XCoef2").toDouble();
            padJB_XCoef[2] = qsetSystemInit->value("JB_XCoef3").toDouble();
            padJB_XCoef[3] = qsetSystemInit->value("JB_XCoef4").toDouble();
            padJB_XCoef[4] = qsetSystemInit->value("JB_XCoef5").toDouble();
            padJB_XCoef[5] = qsetSystemInit->value("JB_XCoef6").toDouble();
        }
        else
        {
            delete qsetSystemInit;
            return;
        }
        qsetSystemInit->endGroup();
        /// [JB_YCoef]
        qsetSystemInit->beginGroup("JB_YCoef");
        if (qsetSystemInit->contains("JB_YCoef1")
            && qsetSystemInit->contains("JB_YCoef2")
            && qsetSystemInit->contains("JB_YCoef3")
            && qsetSystemInit->contains("JB_YCoef4")
            && qsetSystemInit->contains("JB_YCoef5")
            && qsetSystemInit->contains("JB_YCoef6"))
        {
            padJB_YCoef[0] = qsetSystemInit->value("JB_YCoef1").toDouble();
            padJB_YCoef[1] = qsetSystemInit->value("JB_YCoef2").toDouble();
            padJB_YCoef[2] = qsetSystemInit->value("JB_YCoef3").toDouble();
            padJB_YCoef[3] = qsetSystemInit->value("JB_YCoef4").toDouble();
            padJB_YCoef[4] = qsetSystemInit->value("JB_YCoef5").toDouble();
            padJB_YCoef[5] = qsetSystemInit->value("JB_YCoef6").toDouble();
        }
        else
        {
            delete qsetSystemInit;
            return;
        }
        qsetSystemInit->endGroup();
    }
}

int ImageProcAlgo::ErodeBW(cl_mem cmInput, cl_mem cmOutput, int iHSize, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ckBWErode8, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckBWErode8, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckBWErode8, 2, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckBWErode8, 3, sizeof(unsigned int), &szImageHeight);
    clSetKernelArg(m_ckBWErode8, 4, sizeof(unsigned int), &iHSize);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckBWErode8, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

int ImageProcAlgo::ErodeGray(cl_mem cmInput, cl_mem cmOutput, int iHSize, size_t szImageWidth, size_t szImageHeight)
{
    cl_int ciErrNum = CL_SUCCESS;
    size_t paszLocalWorkSize[2];
    paszLocalWorkSize[0] = 16;
    paszLocalWorkSize[1] = 16;
    size_t paszGlobalWorkSize[2];
    paszGlobalWorkSize[0] = RoundUp((int)paszLocalWorkSize[0], szImageWidth);
    paszGlobalWorkSize[1] = RoundUp((int)paszLocalWorkSize[1], szImageHeight);

    clSetKernelArg(m_ckGrayErode16, 0, sizeof(cl_mem), (void*)&cmInput);
    clSetKernelArg(m_ckGrayErode16, 1, sizeof(cl_mem), (void*)&cmOutput);
    clSetKernelArg(m_ckGrayErode16, 2, sizeof(unsigned int), &szImageWidth);
    clSetKernelArg(m_ckGrayErode16, 3, sizeof(unsigned int), &szImageHeight);
    clSetKernelArg(m_ckGrayErode16, 4, sizeof(unsigned int), &iHSize);

    clEnqueueNDRangeKernel(m_pGpuDevMngr->cqCommandQueue, m_ckGrayErode16, 2, NULL, paszGlobalWorkSize, paszLocalWorkSize, 0, NULL, NULL);
    clFinish(m_pGpuDevMngr->cqCommandQueue);

    return 0;
}

void ImageProcAlgo::CalcPointError(double dAzi, double dEle, double& dAziErr, double& dEleErr)
{
    dAziErr = (m_errresPonit.dingxiang+
       m_errresPonit.ixA*cos(dAzi/180*3.1415926)*tan(dEle/180*3.1415926)-
       m_errresPonit.iyA*sin(dAzi/180*3.1415926)*tan(dEle/180*3.1415926)+
       m_errresPonit.zhaozhun*(1.0/cos(dEle/180*3.1415926)-1)+
       m_errresPonit.shuiping*tan(dEle/180*3.1415926)); //方位误差（单位：度）
    dEleErr = (m_errresPonit.lingwei-
       m_errresPonit.iyE*cos(dAzi/180*3.1415926)-
       m_errresPonit.ixE*sin(dAzi/180*3.1415926)-
       m_errresPonit.raodong*cos(dEle/180*3.1415926)-
               m_errresPonit.raodong1*sin(dEle/180*3.1415926)); //高低误差（单位：度）
}

void ImageProcAlgo::CalcDistortionDelta(double dPosX, double dPosY, double &dPosDx, double &dPosDy)
{
    double dblstarx = dPosX - m_pGParam->m_SOpticData.fOptCenterX;
    double dblstary = dPosY - m_pGParam->m_SOpticData.fOptCenterY;
    double dblstarxy = 2 * dblstarx * dblstary;
    double dblr2 = dblstarx * dblstarx + dblstary * dblstary;

    dPosDx = m_dblJB_XCOEF[0] + m_dblJB_XCOEF[1] * dblstarx + m_dblJB_XCOEF[2] * dblstary
            + m_dblJB_XCOEF[3] * (2 * dblstarx * dblstarx + dblr2) + m_dblJB_XCOEF[4] * dblstarxy
            + m_dblJB_XCOEF[5] * dblstarx * dblr2;
    dPosDy = m_dblJB_YCOEF[0] + m_dblJB_YCOEF[1] * dblstarx + m_dblJB_YCOEF[2] * dblstary
            + m_dblJB_YCOEF[3] * (2 * dblstary * dblstary + dblr2) + m_dblJB_YCOEF[4] * dblstarxy
            + m_dblJB_YCOEF[5] * dblstary * dblr2;
}
//*************************************************************//

//        unsigned short* pausNoStar = new unsigned short[m_szImageWidth * m_szImageHeight];
//        clEnqueueReadBuffer(m_pGpuDevMngr->cqCommandQueue, m_cmFlat, CL_TRUE, 0, m_szImageWidth * m_szImageHeight * sizeof(unsigned short), pausNoStar, 0, NULL, NULL);
//        FILE* fp;
//        stringstream ss;
//        ss << "/home/dps/fffffff" << m_ulFrameSeq << ".raw";
//        string  strFileName = ss.str();
//        fp =  fopen(strFileName.c_str(), "wb");
//        fwrite(pausNoStar, sizeof(unsigned short), m_szImageWidth * m_szImageHeight, fp);
//        fclose(fp);
//        delete[] pausNoStar;

//        FILE* fp;
//        stringstream ss;
//        ss << "/home/dps/NoStar8.raw";
//        string  strFileName = ss.str();
//        fp = fopen(strFileName.c_str(), "wb");
//        fwrite(m_paucDisp, sizeof(unsigned char), m_szImageWidth * m_szImageHeight, fp);
//        fclose(fp);

