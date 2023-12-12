#include "ImageProcessor.h"
#include <CL/opencl.h>

ImageProcessor::ImageProcessor(void)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SImageProcessorData.pvoidImageProcessor = (void*)this;
    m_pProcessThread = new StandardThread(CallBackProcess, (void*)this);
    m_pProcessThread->start(QThread::HighPriority);
    m_pImageProc = new ImageProcAlgo;
    m_bProcessMode = true;
    m_bInitTrack = false;
    m_uiGrabWidth = m_pGParam->m_SGrabberData.iFullWidth;
    m_uiGrabHeight = m_pGParam->m_SGrabberData.iFullHeight;
    m_uiCropWidth = m_pGParam->m_SGrabberData.iSubWidth;
    m_uiCropHeight = m_pGParam->m_SGrabberData.iSubHeight;
    m_uiDispWidth = m_pGParam->m_SGrabberData.iFullWidth;
    m_uiDispHeight = m_pGParam->m_SGrabberData.iFullHeight;
    m_iProcMode = PROC_NONE;
    m_iTrackMode = TRACK_INIT;
    m_bCenterMode = m_pGParam->m_SImageProcessorData.bCenterModeInit;
    m_bAutoScale = m_pGParam->m_SImageProcessorData.bAutoScaleInit;
    m_uiScaleMax = m_pGParam->m_SImageProcessorData.uiScaleMaxInit;
    m_uiScaleMin = m_pGParam->m_SImageProcessorData.uiScaleMinInit;
    m_iMaxArea = m_pGParam->m_SImageProcessorData.iMaxAreaInit;
    m_iMinArea = m_pGParam->m_SImageProcessorData.iMinAreaInit;
    m_uiFrameSeq = 0;

    sTrackingSettings settings;
    InitProc(PROC_NONE, TRACK_INIT, m_uiGrabWidth, m_uiGrabHeight, 1, 3072, 3072, false, settings);
    m_pImageProc->InitPhotometry();
    m_pImageProc->SetPhotometry();
}
#include <QDebug>
ImageProcessor::~ImageProcessor()
{
    m_pGParam->m_SImageProcessorData.bCenterModeInit = m_bCenterMode;
    m_pGParam->m_SImageProcessorData.bAutoScaleInit = m_bAutoScale;
    m_pGParam->m_SImageProcessorData.uiScaleMaxInit = m_uiScaleMax;
    m_pGParam->m_SImageProcessorData.uiScaleMinInit = m_uiScaleMin;
    m_pGParam->m_SImageProcessorData.iMaxAreaInit = m_iMaxArea;
    m_pGParam->m_SImageProcessorData.iMinAreaInit = m_iMinArea;

    m_qmEventProcess.lock();
    m_pProcessThread->m_stopRequested = true;
    m_qwcEventProcess.wakeAll();
    m_qmEventProcess.unlock();
    delete m_pProcessThread;
    delete m_pImageProc;

    qDebug() << "ImageProcessor";
}

QWaitCondition ImageProcessor::m_qwcEventProcess;
QMutex ImageProcessor::m_qmEventProcess;

void ImageProcessor::CallBackProcess(void* pvoidThis)
{
    ImageProcessor* pThis = (ImageProcessor*)pvoidThis;
    QMutexLocker locker(&m_qmEventProcess);
    while (!pThis->m_pProcessThread->m_stopRequested)
    {
        m_qwcEventProcess.wait(&m_qmEventProcess);
        if (!pThis->m_pProcessThread->m_stopRequested)
        {
            QMutexLocker locker(&pThis->m_pGParam->m_SImageProcessorData.qmutexProcess);

            pThis->m_pGParam->m_SImageProcessorData.bProcessing = true;
            if (pThis->m_bInitTrack)
            {
                pThis->m_pImageProc->InitTracker(pThis->m_iTrackMode, pThis->m_settings, pThis->m_pGParam->m_SNetMasterControlData.qstrTargetID.toUInt());
                pThis->m_bInitTrack = false;
            }

            qDebug() << "#################################";
            std::chrono::high_resolution_clock::time_point beginTime = std::chrono::high_resolution_clock::now();	///////////////////////////

            sFramePacket framePacket;

            if (pThis->m_bProcessMode)
            {
                framePacket.fExpTime = pThis->m_pGParam->m_SGrabberData.dExposureTime;
                framePacket.fFrameFreq = pThis->m_pGParam->m_SGrabberData.dFrameFrequency;
                framePacket.pairfFOVCenterAE.first = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.dAzimuthDegreeTotalRecv;
                framePacket.pairfFOVCenterAE.second = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.dElevationDegreeTotalRecv;
                unsigned short* pausGrab = new unsigned short[pThis->m_uiGrabWidth * pThis->m_uiGrabHeight];
                memcpy(pausGrab, pThis->m_pGParam->m_SGrabberData.pausGrabDataCurrent, pThis->m_uiGrabWidth * pThis->m_uiGrabHeight * sizeof(unsigned short));
                framePacket.sausImageGrab.pvoidBuffer = (void*)pausGrab;
                framePacket.ulFrameSeq = pThis->m_uiFrameSeq++;
                framePacket.stimeFrame.iYear = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iYMDYear;
                framePacket.stimeFrame.iMonth = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iYMDMonth;
                framePacket.stimeFrame.iDay = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iYMDDay;
                framePacket.stimeFrame.iHour = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iHourRecv;
                framePacket.stimeFrame.iMinute = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMinuteRecv;
                framePacket.stimeFrame.iSecond = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iSecondRecv;
                framePacket.stimeFrame.iMillisecond = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMilliSecondRecv;
                framePacket.stimeFrame.iMicrosecond = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMicroSecondRecv;
                framePacket.fTemp = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.fTemp;
                framePacket.fAtmosP = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.fAtmosP;
                framePacket.fHumidity = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.fHumidity;

                pThis->m_pGParam->m_SImageProcessorData.sexpdispdataProc = pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab;  // use to image storage
                pThis->MarkDispInfor();

                pThis->m_pImageProc->InputFramePacket(framePacket);
                emit pThis->SignalGrabDataRotate();
                pThis->m_pImageProc->ProcFramePacket();

                if (pausGrab)   delete []  pausGrab;

                emit pThis->SignalSend();
            }
            else
            {
                framePacket.fExpTime = pThis->m_pGParam->m_SImageReplayerData.dExposureTimeDisp;
                framePacket.fFrameFreq = pThis->m_pGParam->m_SImageReplayerData.dFrameFrequencyDisp;
                framePacket.pairfFOVCenterAE.first = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dAzimuthDegreeTotalRecv;
                framePacket.pairfFOVCenterAE.second = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dElevationDegreeTotalRecv;
                unsigned short* pausReplay = new unsigned short[pThis->m_uiGrabWidth * pThis->m_uiGrabHeight];
                memcpy(pausReplay, pThis->m_pGParam->m_SImageReplayerData.pausReplayData, pThis->m_uiGrabWidth * pThis->m_uiGrabHeight * sizeof(unsigned short));
                framePacket.sausImageGrab.pvoidBuffer = (void*)pausReplay;
                framePacket.ulFrameSeq = pThis->m_uiFrameSeq++;
                framePacket.stimeFrame.iYear = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDYear;
                framePacket.stimeFrame.iMonth = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDMonth;
                framePacket.stimeFrame.iDay = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDDay;
                framePacket.stimeFrame.iHour = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iHourRecv;
                framePacket.stimeFrame.iMinute = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMinuteRecv;
                framePacket.stimeFrame.iSecond = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iSecondRecv;
                framePacket.stimeFrame.iMillisecond = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMilliSecondRecv;
                framePacket.stimeFrame.iMicrosecond = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMicroSecondRecv;
                framePacket.fTemp = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fTemp;
                framePacket.fAtmosP = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fAtmosP;
                framePacket.fHumidity = pThis->m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fHumidity;

                pThis->MarkDispInfor();

                pThis->m_pImageProc->InputFramePacket(framePacket);
                pThis->m_pImageProc->ProcFramePacket();
                if (pausReplay)   delete []  pausReplay;
            }

            emit pThis->SignalTrackData();
            emit pThis->SignalDisplay();
            pThis->m_pGParam->m_SImageProcessorData.bProcessing = false;
            std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();	///////////////////////////
            std::chrono::milliseconds timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime);	///////////////////////////
            QDateTime qdtCut = QDateTime::currentDateTime();
            qDebug() << "### Proc:" << timeInterval.count() << "ms"
                     << " *** " << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iHourRecv
                     << ":" << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMinuteRecv
                     << ":" << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iSecondRecv
                     << "." << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMilliSecondRecv
                     << " *** " << qdtCut.time().hour()
                     << ":" << qdtCut.time().minute()
                     << ":" << qdtCut.time().second()
                     << "." << qdtCut.time().msec();
            qDebug() << "#################################";
        }
    }
}

int ImageProcessor::InitProc(int iProcMode, int iTrackMode, size_t szGrabWidth, size_t szGrabHeight, int iBinning, size_t szCropWidth, size_t szCropHeight, bool bRotate, sTrackingSettings trackSettings)
{
    if (iProcMode == PROC_NONE
        || (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
        || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_MANUAL))
    {
        QMutexLocker locker(&m_pGParam->m_SImageProcessorData.qmutexProcess);

        m_pImageProc->InitProc(iProcMode, iTrackMode, szGrabWidth, szGrabHeight, iBinning, szCropWidth, szCropHeight, bRotate, trackSettings);
        m_iProcMode = iProcMode;
        m_iTrackMode = iTrackMode;
        m_uiGrabWidth = szGrabWidth;
        m_uiGrabHeight = szGrabHeight;
        m_uiCropWidth = szCropWidth;
        m_uiCropHeight = szCropHeight;
        m_uiDispWidth = szGrabWidth;
        m_uiDispHeight = szGrabHeight;
        m_uiFrameSeq = 0;
    }
    else
    {
        return 1;
    }

    return 0;
}

void ImageProcessor::SetCenterMode(bool bCenterMode)
{
    m_bCenterMode = bCenterMode;
    m_pImageProc->SetCenterMode(m_bCenterMode);
}

void ImageProcessor::SetBlobAreaLimit(int iMinArea, int iMaxArea)
{
    if (iMinArea > 0 && iMaxArea > 0 && iMaxArea > iMinArea)
    {
        m_iMinArea = iMinArea;
        m_iMaxArea = iMaxArea;
        m_pImageProc->SetBlobAreaLimit(m_iMinArea, m_iMaxArea);
    }
}

void ImageProcessor::SetScale(bool bAutoScale, unsigned int uiScaleMax, unsigned int uiScaleMin)
{
    if (bAutoScale
        || (!bAutoScale && uiScaleMin >= 0 && uiScaleMax >= 0 && uiScaleMax > uiScaleMin))
    {
        m_bAutoScale = bAutoScale;
        m_uiScaleMax = uiScaleMax;
        m_uiScaleMin = uiScaleMin;
        m_pImageProc->SetScale(bAutoScale, uiScaleMax, uiScaleMin);
    }
}

void ImageProcessor::SetDispEnhance(bool bDispEnhance)
{
    m_pImageProc->SetDispEnhance(bDispEnhance);
}

void ImageProcessor::SetDispBW(bool bDispBW)
{
    m_pImageProc->SetDispBW(bDispBW);
}

void ImageProcessor::GetFramePacket(vector<sFramePacket>& vectFramePacket)
{
    m_pImageProc->GetFramePacket(vectFramePacket);
}

int ImageProcessor::GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo)
{
    m_pImageProc->GetVectTargetInfo(vectTargetInfo);
}

int ImageProcessor::GetTargetInfo(sTargetInfo& targetInfo)
{
    m_pImageProc->GetTargetInfo(targetInfo);
}

unsigned char* ImageProcessor::GetDispImage(unsigned int& uiWidth, unsigned int& uiHeight)
{
    uiWidth = m_uiDispWidth;
    uiHeight = m_uiDispHeight;

    return m_pImageProc->GetDispImage();
}

int ImageProcessor::InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID)
{
    m_iTrackMode = iTrackMode;
    m_settings = settings;
    m_bInitTrack = true;
    m_iTargetID = iTargetID;
}

void ImageProcessor::on_SignalGrabData(void)
{
    m_bProcessMode = true;
    m_pGParam->m_SImageProcessorData.bProcessMode = true;
    if(!m_pGParam->m_SImageProcessorData.bProcessing)
        m_qwcEventProcess.wakeAll();
    else
        qDebug() << "GrabImg is processing!";
}

void ImageProcessor::on_SignalReplayData(void)
{
    m_bProcessMode = false;
    m_pGParam->m_SImageProcessorData.bProcessMode = false;
    if(!m_pGParam->m_SImageProcessorData.bProcessing)
        m_qwcEventProcess.wakeAll();
    else
        qDebug() << "ReplayImg is processing!";
}

void ImageProcessor::on_SignalDispStatus()
{
    m_pImageProc->DispMemcpy();
    emit SignalDisplay();
}

void ImageProcessor::MarkDispInfor(void)
{
    if (m_bProcessMode)
    {
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDYear = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDYear;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDMonth = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDMonth;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDDay = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDDay;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iHourRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iHourRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMinuteRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMinuteRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMilliSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMilliSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMicroSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMicroSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dAzimuthDegreeTotalRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.dAzimuthDegreeTotalRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dElevationDegreeTotalRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.dElevationDegreeTotalRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fTemp = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fTemp;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fAtmosP = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fAtmosP;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fHumidity = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fHumidity;
        m_pGParam->m_SImageProcessorData.dExposureTimeDisp = m_pGParam->m_SCommExposureData.dExposureTimeSend;
        m_pGParam->m_SImageProcessorData.iTriggerModeDisp = m_pGParam->m_SCommExposureData.iTriggerModeSend == TRIGGERMODE_OUT ? 1 : 0;
        m_pGParam->m_SImageProcessorData.dFrameFrequencyDisp = m_pGParam->m_SCommExposureData.iTriggerModeSend == TRIGGERMODE_OUT ? m_pGParam->m_SCommExposureData.dFrameFrequencySend : 0;
    }
    else
    {
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDYear = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDYear;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDMonth = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDMonth;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDDay = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDDay;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iHourRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iHourRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMinuteRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMinuteRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iSecondRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMilliSecondRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMilliSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMicroSecondRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMicroSecondRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dAzimuthDegreeTotalRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dAzimuthDegreeTotalRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dElevationDegreeTotalRecv = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dElevationDegreeTotalRecv;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fTemp = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fTemp;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fAtmosP = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fAtmosP;
        m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fHumidity = m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fHumidity;
        m_pGParam->m_SImageProcessorData.dExposureTimeDisp = m_pGParam->m_SImageReplayerData.dExposureTimeDisp;
        m_pGParam->m_SImageProcessorData.iTriggerModeDisp = m_pGParam->m_SImageReplayerData.iTriggerModeDisp;
        m_pGParam->m_SImageProcessorData.dFrameFrequencyDisp = m_pGParam->m_SImageReplayerData.dFrameFrequencyDisp;
    }
}

void ImageProcessor::on_SignalGrabInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight)
{
    m_uiGrabWidth = uiGrabWidth;
    m_uiGrabHeight = uiGrabHeight;
    sTrackingSettings settings;
    InitProc(m_iProcMode, m_iTrackMode, m_uiGrabWidth, m_uiGrabHeight, 1, m_uiCropWidth, m_uiCropHeight, true, settings);
}

void ImageProcessor::on_SignalReplayInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight)
{
    m_uiGrabWidth = uiGrabWidth;
    m_uiGrabHeight = uiGrabHeight;
    sTrackingSettings settings;
    InitProc(m_iProcMode, m_iTrackMode, m_uiGrabWidth, m_uiGrabHeight, 1, m_uiCropWidth, m_uiCropHeight, false, settings);
}
