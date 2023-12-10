#include "ImageReplayer.h"

ImageReplayer::ImageReplayer(void)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SImageReplayerData.pvoidImageReplayer = (void*)this;
    m_pReplayThread = new StandardThread(CallBackReplay, (void*)this);
    m_pReplayThread->start(QThread::HighPriority);
    m_iReplayInterval = 100;
    m_bReplay = false;
    m_bBrowse = false;
    m_bDirection = true;
    m_bImageValid = false;
    m_uiSeqCurrent = 0;
    m_qstrReplayPath = "";
    m_pqtimerReplay = new QTimer(this);
    connect(m_pqtimerReplay, SIGNAL(timeout()), this, SLOT(on_qtimerReplay_timeout()));
    m_pqtimerReplay->start(m_iReplayInterval);
    m_pGParam->m_SImageReplayerData.pausReplayData = NULL;
}
#include <QDebug>
ImageReplayer::~ImageReplayer()
{
    SetPause();
    m_qmEventReplay.lock();
    m_pReplayThread->m_stopRequested = true;
    m_qwcEventReplay.wakeAll();
    m_qmEventReplay.unlock();
    delete m_pReplayThread;
    if (m_pGParam->m_SImageReplayerData.pausReplayData)
    {
        delete[] m_pGParam->m_SImageReplayerData.pausReplayData;
    }

    qDebug() << "ImageReplayer";
}

QWaitCondition ImageReplayer::m_qwcEventReplay;
QMutex ImageReplayer::m_qmEventReplay;

void ImageReplayer::CallBackReplay(void* pvoidThis)
{
    ImageReplayer* pThis = (ImageReplayer*)pvoidThis;
    QMutexLocker locker(&m_qmEventReplay);
    while (!pThis->m_pReplayThread->m_stopRequested)
    {
        m_qwcEventReplay.wait(&m_qmEventReplay);
        if (!pThis->m_pReplayThread->m_stopRequested)
        {
            if (pThis->m_bDirection)
            {
                if (pThis->m_uiSeqCurrent < pThis->m_qstrlistReplayFileName.size() - 1)
                {
                    pThis->m_uiSeqCurrent++;
                    if (pThis->m_pGParam->m_qstrImageFormat == "raw")
                    {
                        pThis->GetImage();
                    }
                    else if (pThis->m_pGParam->m_qstrImageFormat == "bmp")
                    {
                        pThis->GetBMP(false);
                    }
                }
                else
                {
                    pThis->m_bReplay = false;
                }
            }
            else
            {
                if (pThis->m_uiSeqCurrent > 0)
                {
                    pThis->m_uiSeqCurrent--;
                    if (pThis->m_pGParam->m_qstrImageFormat == "raw")
                    {
                        pThis->GetImage();
                    }
                    else if (pThis->m_pGParam->m_qstrImageFormat == "bmp")
                    {
                        pThis->GetBMP(false);
                    }
                }
                else
                {
                    pThis->m_bReplay = false;
                }
            }
            pThis->m_bBrowse = false;
        }
    }
}

bool ImageReplayer::SetReplayPath(QString qstrReplayPath)
{
    SetPause();
    QStringList qstrlistFilter;
    if (m_pGParam->m_qstrImageFormat == "raw")
        qstrlistFilter << "*.raw";
    else if (m_pGParam->m_qstrImageFormat == "bmp")
        qstrlistFilter << "*.bmp";
    QDir qDirReplay(qstrReplayPath);
    if (qDirReplay.exists())
    {
        m_qstrReplayPath = qstrReplayPath;
        m_qstrlistReplayFileName.clear();
        m_qstrlistReplayFileName = qDirReplay.entryList(qstrlistFilter, QDir::Files | QDir::Readable, QDir::Name);
        for (QStringList::Iterator iter = m_qstrlistReplayFileName.begin(); iter != m_qstrlistReplayFileName.end(); iter++)
        {
            *iter = qstrReplayPath + "/" + *iter;
        }
        m_bReplay = false;
        m_bBrowse = false;
        m_bDirection = true;
        m_iReplayInterval = m_pGParam->m_SGrabberData.bWindowEN ? 100 : 3000;
        m_pqtimerReplay->setInterval(m_iReplayInterval);
        m_uiSeqCurrent = 0;
        if (m_qstrlistReplayFileName.size() > 0)
        {
            if (m_pGParam->m_qstrImageFormat == "raw")
            {
                if (GetHeader())
                    GetImage();
            }
            else if (m_pGParam->m_qstrImageFormat == "bmp")
            {
                if (GetHeaderBMP())
                {
                    GetBMP(true);
                }
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool ImageReplayer::SetReplay(bool bReplayDirection)
{
    if (m_bImageValid)
    {
        if (m_qstrlistReplayFileName.size() > 0)
        {
            m_bReplay = true;
            m_bBrowse = false;
            m_bDirection = bReplayDirection;
        }
        else
        {
            return false;
        }
        return true;
    }
    return false;
}

bool ImageReplayer::SetBrowse(bool bBrowseDirection)
{
    if (m_bImageValid)
    {
        if (m_qstrlistReplayFileName.size() > 0)
        {
            m_bReplay = false;
            m_bBrowse = true;
            m_bDirection = bBrowseDirection;
        }
        else
        {
            return false;
        }
        return true;
    }
    return false;
}

bool ImageReplayer::SetPause(void)
{
    if (m_bImageValid)
    {
        m_bReplay = false;
        m_bBrowse = false;
        return true;
    }
    return false;
}

bool ImageReplayer::GetHeader(void)
{
    SImageFileHeader simagefileheaderRead;
    /// 读取图像文件
    QFile qfileRead(m_qstrlistReplayFileName.at(m_uiSeqCurrent));
    qfileRead.open(QIODevice::ReadOnly);
    qfileRead.read(simagefileheaderRead.acImageWidth, 2);
    qfileRead.read(simagefileheaderRead.acImageHeight, 2);
    unsigned char* paucImageWidth = (unsigned char*)simagefileheaderRead.acImageWidth;
    m_uiImageWidth = (unsigned int)(paucImageWidth[0]) * 256 + (unsigned int)(paucImageWidth[1]);
    unsigned char* paucImageHeight = (unsigned char*)simagefileheaderRead.acImageHeight;
    m_uiImageHeight = (unsigned int)(paucImageHeight[0]) * 256 + (unsigned int)(paucImageHeight[1]);
    qfileRead.read(simagefileheaderRead.acYear, 2);
    qfileRead.read(&simagefileheaderRead.cMonth, 1);
    qfileRead.read(&simagefileheaderRead.cDay, 1);
    qfileRead.read(&simagefileheaderRead.cHour, 1);
    qfileRead.read(&simagefileheaderRead.cMinute, 1);
    qfileRead.read(&simagefileheaderRead.cSecond, 1);
    qfileRead.read(simagefileheaderRead.acMilliSecond, 2);
    qfileRead.read(simagefileheaderRead.acMicroSecond, 2);
    qfileRead.read(simagefileheaderRead.acAzimuth, 3);
    qfileRead.read(simagefileheaderRead.acElevation, 3);
    qfileRead.read(simagefileheaderRead.acExposureTime, 3);
    qfileRead.read(&simagefileheaderRead.cTriggerMode, 1);
    qfileRead.read(simagefileheaderRead.acFrameFrequency, 2);
    qfileRead.read(&simagefileheaderRead.cTemp, 1);
    qfileRead.read(simagefileheaderRead.acAtmosP, 2);
    qfileRead.read(&simagefileheaderRead.cHumidty, 1);

    unsigned int uiWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    if (m_uiImageWidth == uiWidth && m_uiImageWidth == uiHeight)
    {
        if (m_pGParam->m_SImageReplayerData.pausReplayData) delete [] m_pGParam->m_SImageReplayerData.pausReplayData;
        m_pGParam->m_SImageReplayerData.pausReplayData = new unsigned short[m_uiImageWidth * m_uiImageHeight];
        memset(m_pGParam->m_SImageReplayerData.pausReplayData, 0, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));

        emit SignalReplayInit(m_uiImageWidth, m_uiImageHeight);
        SetReplayInterval(simagefileheaderRead.acFrameFrequency);
    }
    else if (m_uiImageWidth > uiWidth && m_uiImageWidth > uiHeight)
    {
        emit SignalChangeUITrackMode(0);
        return false;
    }
    else if (m_uiImageWidth < uiWidth && m_uiImageWidth < uiHeight)
    {
        emit SignalChangeUITrackMode(1);
        return false;
    }
    else
    {
        emit SignalChangeUITrackMode(2);
        return false;
    }

    return true;
}

void ImageReplayer::GetImage(void)
{
    SImageFileHeader simagefileheaderRead;
    /// 读取图像文件
    QFile qfileRead(m_qstrlistReplayFileName.at(m_uiSeqCurrent));
    qfileRead.open(QIODevice::ReadOnly);
    qfileRead.read(simagefileheaderRead.acImageWidth, 2);
    qfileRead.read(simagefileheaderRead.acImageHeight, 2);
    unsigned char* paucImageWidth = (unsigned char*)simagefileheaderRead.acImageWidth;
    unsigned int uiImageWidth = (unsigned int)(paucImageWidth[0]) * 256 + (unsigned int)(paucImageWidth[1]);
    unsigned char* paucImageHeight = (unsigned char*)simagefileheaderRead.acImageHeight;
    unsigned int uiImageHeight = (unsigned int)(paucImageHeight[0]) * 256 + (unsigned int)(paucImageHeight[1]);
    if (uiImageWidth == m_uiImageWidth    // 判断回放图像宽高是否与初始化参数一致
        && uiImageHeight == m_uiImageHeight)
    {
        qfileRead.read(simagefileheaderRead.acYear, 2);
        qfileRead.read(&simagefileheaderRead.cMonth, 1);
        qfileRead.read(&simagefileheaderRead.cDay, 1);
        qfileRead.read(&simagefileheaderRead.cHour, 1);
        qfileRead.read(&simagefileheaderRead.cMinute, 1);
        qfileRead.read(&simagefileheaderRead.cSecond, 1);
        qfileRead.read(simagefileheaderRead.acMilliSecond, 2);
        qfileRead.read(simagefileheaderRead.acMicroSecond, 2);
        qfileRead.read(simagefileheaderRead.acAzimuth, 3);
        qfileRead.read(simagefileheaderRead.acElevation, 3);
        qfileRead.read(simagefileheaderRead.acExposureTime, 3);
        qfileRead.read(&simagefileheaderRead.cTriggerMode, 1);
        qfileRead.read(simagefileheaderRead.acFrameFrequency, 2);
        qfileRead.read(&simagefileheaderRead.cTemp, 1);
        qfileRead.read(simagefileheaderRead.acAtmosP, 2);
        qfileRead.read(&simagefileheaderRead.cHumidty, 1);

        unsigned short* pausRead = new unsigned short[m_uiImageWidth * m_uiImageHeight];
        qfileRead.read((char*)pausRead, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));
        /// 写入全局数据
        {
            QMutexLocker locker(&m_pGParam->m_SImageReplayerData.qmutexReplay);
            unsigned char* paucYear = (unsigned char*)simagefileheaderRead.acYear;
            unsigned char* paucMonth = (unsigned char*)&simagefileheaderRead.cMonth;
            unsigned char* paucDay = (unsigned char*)&simagefileheaderRead.cDay;
            unsigned char* paucHour = (unsigned char*)&simagefileheaderRead.cHour;
            unsigned char* paucMinute = (unsigned char*)&simagefileheaderRead.cMinute;
            unsigned char* paucSecond = (unsigned char*)&simagefileheaderRead.cSecond;
            unsigned char* paucMilliSecond = (unsigned char*)simagefileheaderRead.acMilliSecond;
            unsigned char* paucMicroSecond = (unsigned char*)simagefileheaderRead.acMicroSecond;
            unsigned char* paucAzimuth = (unsigned char*)simagefileheaderRead.acAzimuth;
            unsigned char* paucElevation = (unsigned char*)simagefileheaderRead.acElevation;
            unsigned char* paucExposureTime = (unsigned char*)simagefileheaderRead.acExposureTime;
            unsigned char* paucTriggerMode = (unsigned char*)&simagefileheaderRead.cTriggerMode;
            unsigned char* paucFrameFrequency = (unsigned char*)simagefileheaderRead.acFrameFrequency;
            unsigned char* paucTemp = (unsigned char*)&simagefileheaderRead.cTemp;
            unsigned char* paucAtmosP = (unsigned char*)simagefileheaderRead.acAtmosP;
            unsigned char* paucHumidty = (unsigned char*)&simagefileheaderRead.cHumidty;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDYear =  (unsigned int)(paucYear[0]) * 256 +  (unsigned int)(paucYear[1]);
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDMonth = *paucMonth;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDDay = *paucDay;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iHourRecv = *paucHour;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMinuteRecv = *paucMinute;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iSecondRecv = *paucSecond;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMilliSecondRecv = (unsigned int)(paucMilliSecond[0]) * 256 +  (unsigned int)(paucMilliSecond[1]);
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMicroSecondRecv =  (unsigned int)(paucMicroSecond[0]) * 256 +  (unsigned int)(paucMicroSecond[1]);
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dAzimuthDegreeTotalRecv = ( (unsigned int)(paucAzimuth[0]) * 65536 +  (unsigned int)(paucAzimuth[1]) * 256 +  (unsigned int)(paucAzimuth[2])) / 10000.0;
            m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dElevationDegreeTotalRecv = ( (unsigned int)(paucElevation[0]) * 65536 +  (unsigned int)(paucElevation[1]) * 256 +  (unsigned int)(paucElevation[2])) / 10000.0;
            if (*paucTemp != 0)
                m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fTemp = *paucTemp - 100.0;
            if (paucAtmosP[0] != 0 && paucAtmosP[1] != 0)
                m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fAtmosP = ((unsigned int)(paucAtmosP[0]) * 256 +  (unsigned int)(paucAtmosP[1])) * 10.0;
            if (*paucHumidty != 0)
                m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fHumidity = *paucHumidty / 100.0;
            m_pGParam->m_SImageReplayerData.dExposureTimeDisp = ( (unsigned int)(paucExposureTime[0]) * 65536 +  (unsigned int)(paucExposureTime[1]) * 256 +  (unsigned int)(paucExposureTime[2])) / 1000.0;
            m_pGParam->m_SImageReplayerData.iTriggerModeDisp = *paucTriggerMode;
            m_pGParam->m_SImageReplayerData.dFrameFrequencyDisp = (double(paucFrameFrequency[0] * 256) + double(paucFrameFrequency[1])) / 1000.0;

            memcpy(m_pGParam->m_SImageReplayerData.pausReplayData, pausRead, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));
        }
        if (pausRead)
        {
            delete[] pausRead;
        }
        m_bImageValid = true;
        emit SignalReplayData();
    }
    else
    {
        m_bImageValid = false;
        m_bReplay = false;
        m_bBrowse = false;
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("回放图像宽度或高度与系统参数不匹配.\n回放失败."));
    }
}

bool ImageReplayer::GetHeaderBMP()
{
    ImgHeadBMP head;
    QString qstrFileName = m_qstrlistReplayFileName.at(m_uiSeqCurrent);

    bool bValid = ImageCode::ReadBmpFormatHeader(qstrFileName.toLocal8Bit().data(), head);

    if (bValid)
    {
        m_uiImageWidth = head.uiWidth;
        m_uiImageHeight = head.uiHeight;
        unsigned int uiWidth = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
        unsigned int uiHeight = m_pGParam->m_SGrabberData.bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
        if (m_uiImageWidth == uiWidth && m_uiImageWidth == uiHeight)
        {
            if (m_pGParam->m_SImageReplayerData.pausReplayData) delete [] m_pGParam->m_SImageReplayerData.pausReplayData;
            m_pGParam->m_SImageReplayerData.pausReplayData = new unsigned short[m_uiImageWidth * m_uiImageHeight];
            memset(m_pGParam->m_SImageReplayerData.pausReplayData, 0, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));

            emit SignalReplayInit(m_uiImageWidth, m_uiImageHeight);
            SetReplayInterval(head.dFrameRate);
        }
        else if (m_uiImageWidth > uiWidth && m_uiImageWidth > uiHeight)
        {
            emit SignalChangeUITrackMode(0);
            return false;
        }
        else if (m_uiImageWidth < uiWidth && m_uiImageWidth < uiHeight)
        {
            emit SignalChangeUITrackMode(1);
            return false;
        }
        else
        {
            emit SignalChangeUITrackMode(2);
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

void ImageReplayer::GetBMP(bool bFirstGetBMP)
{
    ImgHeadBMP head;
    QString qstrFileName = m_qstrlistReplayFileName.at(m_uiSeqCurrent);
    unsigned short* pausRead = new unsigned short[m_uiImageWidth * m_uiImageHeight];
    memset(pausRead, 0, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));
    bool bValid = ImageCode::ReadBmpFormat(qstrFileName.toLocal8Bit().data(), m_uiImageWidth, m_uiImageHeight, (char*)pausRead, head);
    if (bValid)
    {
        /// 改变帧频
        SetReplayInterval(head.dFrameRate);

        QMutexLocker locker(&m_pGParam->m_SImageReplayerData.qmutexReplay);
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDYear = head.uiYear;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDMonth = head.uiMonth;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iYMDDay = head.uiDay;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iHourRecv = head.uiHour;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMinuteRecv = head.uiMinute;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iSecondRecv = head.uiSecond;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMilliSecondRecv = head.uiMSec;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.iMicroSecondRecv = head.uiUSec;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dAzimuthDegreeTotalRecv = head.dAziDeg;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.dElevationDegreeTotalRecv = head.dEleDeg;
        m_pGParam->m_SImageReplayerData.dExposureTimeDisp = head.dExposure;
        m_pGParam->m_SImageProcessorData.bFullLEO = head.dExposure <= 500.0;            /// Full LEO
        m_pGParam->m_SImageReplayerData.iTriggerModeDisp = TRIGGERMODE_FREE;
        m_pGParam->m_SImageReplayerData.dFrameFrequencyDisp = head.dFrameRate;
        head.dTemp = (unsigned int)(head.dTemp * 10.0) > 32767 ? (32767 - ((unsigned int)(head.dTemp * 10.0) - 32768) - 1) / -10.0 : head.dTemp;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fTemp = head.dTemp;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fAtmosP = head.dAtmosP;
        m_pGParam->m_SImageReplayerData.sexpdispdataDisp.fHumidity = head.dHumidity;

        int lastSlashIndex = qstrFileName.lastIndexOf('/');
        int lastDotIndex = qstrFileName.lastIndexOf('.');
//        QString fileNameWithoutExtension = qstrFileName.mid(lastSlashIndex + 1, lastDotIndex - lastSlashIndex - 1);
        QString fileNameWithoutExtension = qstrFileName.left(lastDotIndex);
        m_pGParam->m_SImageReplayerData.qstrCurFileName = fileNameWithoutExtension;

        if (bFirstGetBMP)
        {
            m_pGParam->m_SImageReplayerData.qstrTeleID.sprintf("%04d", head.uiTeleID);
            m_pGParam->m_SImageReplayerData.qstrTargetID.sprintf("%06d", head.uiTargetID);
            m_pGParam->m_SImageReplayerData.qstrStartTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                                                  head.uiYear, head.uiMonth, head.uiDay,
                                                                  head.uiHour, head.uiMinute, head.uiSecond);
        }

        memcpy(m_pGParam->m_SImageReplayerData.pausReplayData, pausRead, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));

        m_bImageValid = true;

        emit SignalReplayData();
    }
    else
    {
        m_bImageValid = false;
        m_bReplay = false;
        m_bBrowse = false;
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("回放图像宽度或高度与系统参数不匹配\n回放失败"));
    }
    if (pausRead)
    {
        delete[] pausRead;
    }
}


void ImageReplayer::SetReplayInterval(char* pacFrameFrequency)
{
    unsigned char* paucFrameFrequency = (unsigned char*)pacFrameFrequency;
    double dFrameFrequency = (double(paucFrameFrequency[0] * 256) + double(paucFrameFrequency[1])) / 1000.0;
    if (dFrameFrequency > 0 )
    {
        if (int(1000.0 / dFrameFrequency) != m_iReplayInterval)
        {
            m_iReplayInterval = int(1000.0 / dFrameFrequency);
            m_pqtimerReplay->setInterval(m_iReplayInterval);
        }
    }
}

void ImageReplayer::SetReplayInterval(double dFrameFrequency)
{
    if (dFrameFrequency > 0 )
    {
        if (int(1000.0 / dFrameFrequency) != m_iReplayInterval)
        {
            m_iReplayInterval = int(1000.0 / dFrameFrequency);
            m_pqtimerReplay->setInterval(m_iReplayInterval);
        }
    }
}

void ImageReplayer::on_qtimerReplay_timeout(void)
{
    if (m_bReplay || m_bBrowse)
    {
        m_qwcEventReplay.wakeAll();
    }
}

QString ImageReplayer::GetReplayPath(void)
{
    return m_qstrReplayPath;
}
















