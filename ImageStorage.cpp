#include "ImageStorage.h"

ImageStorage::ImageStorage(void)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SImageStorageData.pvoidImageStorage = (void*)this;
    m_pStoreThread = new StandardThread(CallBackStore, (void*)this);
    m_pStoreThread->start(QThread::HighPriority);
    m_bStartGrab = false;
    memset(m_simagefileheaderWrite.acImageWidth, 0, sizeof(m_simagefileheaderWrite.acImageWidth));
    memset(m_simagefileheaderWrite.acImageHeight, 0, sizeof(m_simagefileheaderWrite.acImageHeight));
    memset(m_simagefileheaderWrite.acYear, 0, sizeof(m_simagefileheaderWrite.acYear));
    m_simagefileheaderWrite.cMonth = 0;
    m_simagefileheaderWrite.cDay = 0;
    m_simagefileheaderWrite.cHour = 0;
    m_simagefileheaderWrite.cMinute = 0;
    m_simagefileheaderWrite.cSecond = 0;
    memset(m_simagefileheaderWrite.acMilliSecond, 0, sizeof(m_simagefileheaderWrite.acMilliSecond));
    memset(m_simagefileheaderWrite.acMicroSecond, 0, sizeof(m_simagefileheaderWrite.acMicroSecond));
    memset(m_simagefileheaderWrite.acAzimuth, 0, sizeof(m_simagefileheaderWrite.acAzimuth));
    memset(m_simagefileheaderWrite.acElevation, 0, sizeof(m_simagefileheaderWrite.acElevation));
    memset(m_simagefileheaderWrite.acExposureTime, 0, sizeof(m_simagefileheaderWrite.acExposureTime));
    m_simagefileheaderWrite.cTriggerMode = 0;
    memset(m_simagefileheaderWrite.acFrameFrequency, 0, sizeof(m_simagefileheaderWrite.acFrameFrequency));
    m_qstrRootPath = m_pGParam->m_SPath.qstrDataStorePath;
    m_qstrStorePath = "";
    m_bMatchTimeStore = true;
    m_bStore = false;
    m_uiImageWidth = 0;
    m_uiImageHeight = 0;
    m_ullStorageSeq = 0;
}
#include <QDebug>
ImageStorage::~ImageStorage()
{
    if (m_vectFIFO.size() > 0)
    {
        for (std::vector<sStoragePacket*>::iterator iter = m_vectFIFO.begin(); iter != m_vectFIFO.end(); iter++)
        {
            unsigned short* pausImage = (*iter)->pausImage;
            if (pausImage)  delete [] pausImage;
            delete (*iter);
        }
    }

    delete m_pStoreThread;

    qDebug() << "ImageStorage";
}

QWaitCondition ImageStorage::m_qwcEventStore;
QMutex ImageStorage::m_qmEventStore;

//void ImageStorage::CallBackStore(void* pvoidThis)
//{
//    ImageStorage* pThis = (ImageStorage*)pvoidThis;

//    if (pThis->m_vectFIFO.size() > 0)
//    {
//        QDir qDirMake;
//        if (!qDirMake.exists(pThis->m_qstrStorePath))
//        {
//            qDirMake.mkpath(pThis->m_qstrStorePath);
//        }

//        sStoragePacket* packet = pThis->m_vectFIFO[0];

//        qDebug() << "GenerateHeader";
//        pThis->GenerateHeader(packet);
//        qDebug() << "StoreImageData";
//        pThis->StoreImageData(packet);

//        qDebug() << packet->qstrFileName;

//        unsigned short* pausImage = packet->pausImage;
//        if (pausImage)
//            delete [] pausImage;
//        std::vector<sStoragePacket*>::iterator iter = pThis->m_vectFIFO.begin();
//        pThis->m_vectFIFO.erase(iter);
//    }

//    QThread::msleep(1);
//}

void ImageStorage::CallBackStore(void* pvoidThis)
{
    ImageStorage* pThis = (ImageStorage*)pvoidThis;

    if (pThis->m_vectFIFO.size() > 0)
    {
        QDir qDirMake;
        if (!qDirMake.exists(pThis->m_qstrStorePath))
        {
            qDirMake.mkpath(pThis->m_qstrStorePath);
        }
        pThis->WriteIFM();

        sStoragePacket* packet = pThis->m_vectFIFO[0];

        if (pThis->m_pGParam->m_qstrImageFormat == "raw")
        {
            qDebug() << "GenerateHeader";
            pThis->GenerateHeader(packet);
            qDebug() << "StoreImageData";
            pThis->StoreImageData(packet);
        }
        else if (pThis->m_pGParam->m_qstrImageFormat == "bmp")
        {
            ImgHeadBMP head;
            BITMAPFILEHEADER bmpFH;
            BITMAPINFOHEADER bmpIH;
            RGBQUAD pbmpRQ[256];
            char infohead_data[BMP_INFO_SIZE];

            head.uiTargetID = (unsigned int)pThis->m_pGParam->m_SNetMasterControlData.qstrTargetID.toUInt();
            head.uiTeleID = (unsigned int)pThis->m_pGParam->m_SObsParams.iObsID;
            head.uiYear = packet->sexpdispdataCurrent.iYMDYear;
            head.uiMonth = packet->sexpdispdataCurrent.iYMDMonth;
            head.uiDay = packet->sexpdispdataCurrent.iYMDDay;
            head.uiHour = packet->sexpdispdataCurrent.iHourRecv;
            head.uiMinute = packet->sexpdispdataCurrent.iMinuteRecv;
            head.uiSecond = packet->sexpdispdataCurrent.iSecondRecv;
            head.uiMSec = packet->sexpdispdataCurrent.iMilliSecondRecv;
            head.uiUSec = packet->sexpdispdataCurrent.iMicroSecondRecv;
            head.iZTCode = 0x46;
            head.dDeltaX = pThis->m_pGParam->m_SOpticData.fOptCenterX - (double)pThis->m_uiImageWidth;
            head.dDeltaY = pThis->m_pGParam->m_SOpticData.fOptCenterY - (double)pThis->m_uiImageHeight;
            head.dPixelScaleX = 10;
            head.dPixelScaleY = 10;
            head.dAziDeg = packet->sexpdispdataCurrent.dAzimuthDegreeTotalRecv;
            head.dEleDeg = packet->sexpdispdataCurrent.dElevationDegreeTotalRecv;
            head.dDist = 0;
            head.dFocal = head.dPixelScaleX / tan(pThis->m_pGParam->m_SOpticData.fPixelScale/180.0*3.1415926) * 0.001;
            head.dFrameRate = packet->dFrameFrequencyCurrent;
            head.dExposure = packet->dExposureTimeCurrent;
            head.uiWidth = pThis->m_uiImageWidth;
            head.uiHeight = pThis->m_uiImageHeight;
            head.uiPixelColor = 1;
            head.uiPixelBit = 16;
            head.uiBitDepth = 16;
            head.dTemp = pThis->m_pGParam->m_sNetAtmosData.fTemp;
            head.dHumidity = pThis->m_pGParam->m_sNetAtmosData.fHumidity;
            head.dAtmosP = pThis->m_pGParam->m_sNetAtmosData.fAtmosP;
            head.dWindSpeed = 0;
            head.dCL = 0;

            ImageCode::EncodeBmpFileHeader(head, bmpFH);
            ImageCode::EncodeBmpInfoHeader(head, bmpIH);
            ImageCode::EncodeRGBQUAD(head, pbmpRQ);
            ImageCode::EncodeBmpAtt(head, infohead_data);

            ImageCode::WriteBmpFormat(packet->qstrFileName.toLocal8Bit().data(), &bmpFH, &bmpIH, pbmpRQ, infohead_data, BMP_INFO_SIZE, (char*)packet->pausImage,
                head.uiWidth * head.uiHeight * head.uiPixelColor * head.uiPixelBit / 8);
        }

        qDebug() << packet->qstrFileName;

        unsigned short* pausImage = packet->pausImage;
        if (pausImage)
            delete [] pausImage;
        std::vector<sStoragePacket*>::iterator iter = pThis->m_vectFIFO.begin();
        pThis->m_vectFIFO.erase(iter);
    }

    QThread::msleep(1);
}

void ImageStorage::GenerateHeader(sStoragePacket* packet)
{
    m_simagefileheaderWrite.acImageWidth[0] = (m_uiImageWidth >> 8) & 0xFF;
    m_simagefileheaderWrite.acImageWidth[1] = m_uiImageWidth & 0xFF;
    m_simagefileheaderWrite.acImageHeight[0] = (m_uiImageHeight >> 8) & 0xFF;
    m_simagefileheaderWrite.acImageHeight[1] = m_uiImageHeight & 0xFF;
    m_simagefileheaderWrite.acYear[0] = (packet->sexpdispdataCurrent.iYMDYear >> 8) & 0xFF;
    m_simagefileheaderWrite.acYear[1] = packet->sexpdispdataCurrent.iYMDYear & 0xFF;
    m_simagefileheaderWrite.cMonth = packet->sexpdispdataCurrent.iYMDMonth;
    m_simagefileheaderWrite.cDay = packet->sexpdispdataCurrent.iYMDDay;
    m_simagefileheaderWrite.cHour = packet->sexpdispdataCurrent.iHourRecv;
    m_simagefileheaderWrite.cMinute = packet->sexpdispdataCurrent.iMinuteRecv;
    m_simagefileheaderWrite.cSecond = packet->sexpdispdataCurrent.iSecondRecv;
    m_simagefileheaderWrite.acMilliSecond[0] = (packet->sexpdispdataCurrent.iMilliSecondRecv >> 8) & 0xFF;
    m_simagefileheaderWrite.acMilliSecond[1] = packet->sexpdispdataCurrent.iMilliSecondRecv & 0xFF;
    m_simagefileheaderWrite.acMicroSecond[0] = (packet->sexpdispdataCurrent.iMicroSecondRecv >> 8) & 0xFF;
    m_simagefileheaderWrite.acMicroSecond[1] = packet->sexpdispdataCurrent.iMicroSecondRecv & 0xFF;
    m_simagefileheaderWrite.acAzimuth[0] = ((unsigned long)(packet->sexpdispdataCurrent.dAzimuthDegreeTotalRecv * 10000) >> 16) & 0xFF;
    m_simagefileheaderWrite.acAzimuth[1] = ((unsigned long)(packet->sexpdispdataCurrent.dAzimuthDegreeTotalRecv * 10000) >> 8) & 0xFF;
    m_simagefileheaderWrite.acAzimuth[2] = (unsigned long)(packet->sexpdispdataCurrent.dAzimuthDegreeTotalRecv * 10000) & 0xFF;
    m_simagefileheaderWrite.acElevation[0] = ((unsigned long)(packet->sexpdispdataCurrent.dElevationDegreeTotalRecv * 10000) >> 16) & 0xFF;
    m_simagefileheaderWrite.acElevation[1] = ((unsigned long)(packet->sexpdispdataCurrent.dElevationDegreeTotalRecv * 10000) >> 8) & 0xFF;
    m_simagefileheaderWrite.acElevation[2] = (unsigned long)(packet->sexpdispdataCurrent.dElevationDegreeTotalRecv * 10000) & 0xFF;
    m_simagefileheaderWrite.acExposureTime[0] = ((unsigned long)(packet->dExposureTimeCurrent * 1000) >> 16) & 0xFF;
    m_simagefileheaderWrite.acExposureTime[1] = ((unsigned long)(packet->dExposureTimeCurrent * 1000) >> 8) & 0xFF;
    m_simagefileheaderWrite.acExposureTime[2] = (unsigned long)(packet->dExposureTimeCurrent * 1000) & 0xFF;
    m_simagefileheaderWrite.cTriggerMode = packet->iTriggerModeCurrent;
    m_simagefileheaderWrite.acFrameFrequency[0] = ((unsigned long)(packet->dFrameFrequencyCurrent * 1000.0) >> 8) & 0xFF;
    m_simagefileheaderWrite.acFrameFrequency[1] = (unsigned long)(packet->dFrameFrequencyCurrent * 1000.0) & 0xFF;
    m_simagefileheaderWrite.cTemp = (unsigned int)(packet->sexpdispdataCurrent.fTemp + 100);
    m_simagefileheaderWrite.acAtmosP[0] = ((unsigned int)(packet->sexpdispdataCurrent.fAtmosP / 10.0) >> 8) & 0xFF;
    m_simagefileheaderWrite.acAtmosP[1] = (unsigned int)(packet->sexpdispdataCurrent.fAtmosP / 10.0) & 0xFF;
    m_simagefileheaderWrite.cHumidty = (unsigned int)(packet->sexpdispdataCurrent.fHumidity * 100);
}

void ImageStorage::WriteHeader(char* pacBuffer)
{
    *pacBuffer++ = m_simagefileheaderWrite.acImageWidth[0];
    *pacBuffer++ = m_simagefileheaderWrite.acImageWidth[1];
    *pacBuffer++ = m_simagefileheaderWrite.acImageHeight[0];
    *pacBuffer++ = m_simagefileheaderWrite.acImageHeight[1];
    *pacBuffer++ = m_simagefileheaderWrite.acYear[0];
    *pacBuffer++ = m_simagefileheaderWrite.acYear[1];
    *pacBuffer++ = m_simagefileheaderWrite.cMonth;
    *pacBuffer++ = m_simagefileheaderWrite.cDay;
    *pacBuffer++ = m_simagefileheaderWrite.cHour;
    *pacBuffer++ = m_simagefileheaderWrite.cMinute;
    *pacBuffer++ = m_simagefileheaderWrite.cSecond;
    *pacBuffer++ = m_simagefileheaderWrite.acMilliSecond[0];
    *pacBuffer++ = m_simagefileheaderWrite.acMilliSecond[1];
    *pacBuffer++ = m_simagefileheaderWrite.acMicroSecond[0];
    *pacBuffer++ = m_simagefileheaderWrite.acMicroSecond[1];
    *pacBuffer++ = m_simagefileheaderWrite.acAzimuth[0];
    *pacBuffer++ = m_simagefileheaderWrite.acAzimuth[1];
    *pacBuffer++ = m_simagefileheaderWrite.acAzimuth[2];
    *pacBuffer++ = m_simagefileheaderWrite.acElevation[0];
    *pacBuffer++ = m_simagefileheaderWrite.acElevation[1];
    *pacBuffer++ = m_simagefileheaderWrite.acElevation[2];
    *pacBuffer++ = m_simagefileheaderWrite.acExposureTime[0];
    *pacBuffer++ = m_simagefileheaderWrite.acExposureTime[1];
    *pacBuffer++ = m_simagefileheaderWrite.acExposureTime[2];
    *pacBuffer++ = m_simagefileheaderWrite.cTriggerMode;
    *pacBuffer++ = m_simagefileheaderWrite.acFrameFrequency[0];
    *pacBuffer++ = m_simagefileheaderWrite.acFrameFrequency[1];
    *pacBuffer++ = m_simagefileheaderWrite.cTemp;
    *pacBuffer++ = m_simagefileheaderWrite.acAtmosP[0];
    *pacBuffer++ = m_simagefileheaderWrite.acAtmosP[1];
    *pacBuffer++ = m_simagefileheaderWrite.cHumidty;
}

void ImageStorage::StoreImageData(sStoragePacket* packet)
{
    size_t sizetHeaderLength = 31;
    size_t sizetFileLength = m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short) + sizetHeaderLength;
    char* pacFileData = new char[sizetFileLength];
    WriteHeader(pacFileData);
    memcpy(pacFileData + sizetHeaderLength, packet->pausImage, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));

    FILE* fp;
    fp=fopen(packet->qstrFileName.toStdString().c_str(),"w");
    if (fp)
    {
        fwrite(pacFileData, sizetFileLength, 1, fp);
        fclose(fp);
        m_pGParam->m_SImageStorageData.iStorageStatus == STATUS_OK;
    }
    else
    {
        m_pGParam->m_SImageStorageData.iStorageStatus == STATUS_ERROR;
    }

    if (pacFileData)    delete [] pacFileData;
}

void ImageStorage::WriteIFM()
{
    QString qstrFileName;
    if (m_pGParam->m_SNetMasterControlData.bSearch)
        qstrFileName = m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + "NNNNNN" + "_" + QString::number(m_pGParam->m_SObsParams.iObsID) + ".IMI";
    else
        qstrFileName = m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + m_pGParam->m_SNetMasterControlData.qstrTargetID + "_" + QString::number(m_pGParam->m_SObsParams.iObsID) + ".IMI";
    QString qstrTotalName = m_qstrStorePath + "/" + qstrFileName;
    std::fstream file;
    file.open(qstrTotalName.toLocal8Bit().data(), std::ios::trunc);
    file << "C " << qstrFileName.toLocal8Bit().data() << endl;
    file << "C " << m_pGParam->m_SNetMasterControlData.qstrTaskID.toLocal8Bit().data() << endl;
    file << "C " << m_pGParam->m_SNetMasterControlData.qstrStartTime.toLocal8Bit().data() << endl;
    file << "C" << m_pGParam->m_SNetMasterControlData.qstrEndTime.toLocal8Bit().data() << endl;
    file << "C " << endl;
    file << "C " << QString::number(m_pGParam->m_SObsParams.iObsID).toLocal8Bit().data() << endl;
    file << "C " << endl;
    file << "C " << endl;
    file << "C " << endl;
    file << "C " << endl;
    file << "E" << endl;
    file.close();
}

void ImageStorage::on_SignalGrabDataRotate(void)
{
    qDebug() << "on_SignalGrabDataRotate";
    if (m_bStore)
    {
        sStoragePacket* packet = new sStoragePacket;
        if (m_bMatchTimeStore)	// 匹配时间存储
        {
            /// 抓取曝光数据包
            m_sexpdispdataCurrent.iYMDYear = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDYear;
            m_sexpdispdataCurrent.iYMDMonth = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDMonth;
            m_sexpdispdataCurrent.iYMDDay = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iYMDDay;
            m_sexpdispdataCurrent.iDayRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iDayRecv;
            m_sexpdispdataCurrent.iHourRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iHourRecv;
            m_sexpdispdataCurrent.iMinuteRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMinuteRecv;
            m_sexpdispdataCurrent.iSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iSecondRecv;
            m_sexpdispdataCurrent.iMilliSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMilliSecondRecv;
            m_sexpdispdataCurrent.iMicroSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iMicroSecondRecv;
            m_sexpdispdataCurrent.dAzimuthDegreeTotalRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.dAzimuthDegreeTotalRecv;
            m_sexpdispdataCurrent.iAzimuthDegreeRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iAzimuthDegreeRecv;
            m_sexpdispdataCurrent.iAzimuthMinuteRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iAzimuthMinuteRecv;
            m_sexpdispdataCurrent.iAzimuthSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iAzimuthSecondRecv;
            m_sexpdispdataCurrent.dElevationDegreeTotalRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.dElevationDegreeTotalRecv;
            m_sexpdispdataCurrent.iElevationDegreeRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iElevationDegreeRecv;
            m_sexpdispdataCurrent.iElevationMinuteRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iElevationMinuteRecv;
            m_sexpdispdataCurrent.iElevationSecondRecv = m_pGParam->m_SImageProcessorData.sexpdispdataProc.iElevationSecondRecv;
            m_sexpdispdataCurrent.fTemp = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fTemp;
            m_sexpdispdataCurrent.fAtmosP = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fAtmosP;
            m_sexpdispdataCurrent.fHumidity = m_pGParam->m_SImageProcessorData.sexpdispdataProc.fHumidity;
        }
        else // 不匹配时间存储
        {
            /// 抓取计算机时间
            QDateTime qdtCurrent = QDateTime::currentDateTime();
            m_sexpdispdataCurrent.iYMDYear = qdtCurrent.date().year();
            m_sexpdispdataCurrent.iYMDMonth = qdtCurrent.date().month();
            m_sexpdispdataCurrent.iYMDDay = qdtCurrent.date().day();
            m_sexpdispdataCurrent.iDayRecv = 0;
            m_sexpdispdataCurrent.iHourRecv = qdtCurrent.time().hour();
            m_sexpdispdataCurrent.iMinuteRecv = qdtCurrent.time().minute();
            m_sexpdispdataCurrent.iSecondRecv = qdtCurrent.time().second();
            m_sexpdispdataCurrent.iMilliSecondRecv = qdtCurrent.time().msec();
            m_sexpdispdataCurrent.iMicroSecondRecv = 0;
            m_sexpdispdataCurrent.dAzimuthDegreeTotalRecv = 0;
            m_sexpdispdataCurrent.iAzimuthDegreeRecv = 0;
            m_sexpdispdataCurrent.iAzimuthMinuteRecv = 0;
            m_sexpdispdataCurrent.iAzimuthSecondRecv = 0;
            m_sexpdispdataCurrent.dElevationDegreeTotalRecv = 0;
            m_sexpdispdataCurrent.iElevationDegreeRecv = 0;
            m_sexpdispdataCurrent.iElevationMinuteRecv = 0;
            m_sexpdispdataCurrent.iElevationSecondRecv = 0;            
            m_sexpdispdataCurrent.fTemp = 0;
            m_sexpdispdataCurrent.fAtmosP = 0;
            m_sexpdispdataCurrent.fHumidity = 0;
        }
        packet->sexpdispdataCurrent = m_sexpdispdataCurrent;
        /// 抓取相机配置数据包
        {
            m_iTriggerModeCurrent = m_pGParam->m_SGrabberData.iTriggerMode == TRIGGERMODE_OUT ? 1 : 0;
            m_dFrameFrequencyCurrent = m_pGParam->m_SGrabberData.iTriggerMode == TRIGGERMODE_OUT ? m_pGParam->m_SGrabberData.dFrameFrequency : 0;
            m_dExposureTimeCurrent = m_pGParam->m_SGrabberData.dExposureTime;
        }
        packet->iTriggerModeCurrent = m_iTriggerModeCurrent;
        packet->dFrameFrequencyCurrent = m_dFrameFrequencyCurrent;
        packet->dExposureTimeCurrent = m_dExposureTimeCurrent;
        /// 抓图
        {
    //        QMutexLocker locker(&m_pGParam->m_SGrabberData.qmutexGrab);
            packet->pausImage = new unsigned short[m_uiImageWidth * m_uiImageHeight];
            memcpy(packet->pausImage, m_pGParam->m_SGrabberData.pausGrabDataRotate, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned short));
        }
        /// 存储路径
        QString qstrYYYY, qstrYYYYMM, qstrYYYYMMDD, qstrDatePath;
        qstrDatePath = "";
        if (m_pGParam->m_SNetMasterControlData.qstrStartTime.size() >= 14)
        {
            qstrYYYY = m_pGParam->m_SNetMasterControlData.qstrStartTime.mid(0, 4);
            qstrYYYYMM = m_pGParam->m_SNetMasterControlData.qstrStartTime.mid(0, 6);
            qstrYYYYMMDD = m_pGParam->m_SNetMasterControlData.qstrStartTime.mid(0, 8);
            qstrDatePath = qstrYYYY + "/"
                    + qstrYYYYMM + "/"
                    + qstrYYYYMMDD + "/";
        }
        QString qstrSub;
        if (m_pGParam->m_SNetMasterControlData.bSearch)
            qstrSub = qstrDatePath + m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + "NNNNNN" + "_" + QString::number(m_pGParam->m_SObsParams.iObsID) + ".BMP";
        else
            qstrSub = qstrDatePath + m_pGParam->m_SNetMasterControlData.qstrStartTime + "_" + m_pGParam->m_SNetMasterControlData.qstrTargetID + "_" + QString::number(m_pGParam->m_SObsParams.iObsID) + ".BMP";

        if (m_qstrStorePath != m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub)    // change path, seq reset
        {
            m_qstrStorePath = m_pGParam->m_SPath.qstrDataStorePath + "/" + qstrSub;
            m_ullStorageSeq = 0;
        }

        QString qstrCurrentTime;
        qstrCurrentTime.sprintf("%04d%02d%02d%02d%02d%02d%03d",
                                m_sexpdispdataCurrent.iYMDYear,
                                m_sexpdispdataCurrent.iYMDMonth,
                                m_sexpdispdataCurrent.iYMDDay,
                                m_sexpdispdataCurrent.iHourRecv,
                                m_sexpdispdataCurrent.iMinuteRecv,
                                m_sexpdispdataCurrent.iSecondRecv,
                                m_sexpdispdataCurrent.iMilliSecondRecv);
        QString qstrSeq;
        qstrSeq.sprintf("%06d", (unsigned int)m_ullStorageSeq++);
        packet->qstrFileName = m_qstrStorePath + "/"
                               + qstrCurrentTime + "_"
                               + m_pGParam->m_SNetMasterControlData.qstrTargetID + "_"
                               + QString::number(m_pGParam->m_SObsParams.iObsID) + "_"
                               + qstrSeq;
        if (m_pGParam->m_qstrImageFormat == "raw")
            packet->qstrFileName += ".raw";
        else if (m_pGParam->m_qstrImageFormat == "bmp")
            packet->qstrFileName += ".bmp";

        m_vectFIFO.push_back(packet);
    }
}

void ImageStorage::on_SignalStartGrab(unsigned int uiImageWidth, unsigned int uiImageHeight)
{
    if (m_uiImageWidth != uiImageWidth || m_uiImageHeight != uiImageHeight)
    {
        m_uiImageWidth = uiImageWidth;
        m_uiImageHeight = uiImageHeight;
    }
    m_bStartGrab = true;
}

void ImageStorage::SetMatchTimeStore(bool bMatchTimeStore)
{
    if (!m_bStore)
    {
        m_bMatchTimeStore = bMatchTimeStore;
    }
}

void ImageStorage::SetStore(bool bStore)
{
    m_bStore = bStore;
}

