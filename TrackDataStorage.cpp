#include "TrackDataStorage.h"

TrackDataStorage::TrackDataStorage(void)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_STrackDataStorageData.pvoidTrackDataStorage = (void*)this;
    m_pStoreThread = new StandardThread(CallBackStore, (void*)this);
    m_pStoreThread->start(QThread::HighPriority);
    m_bStore = false;
    m_pImageProcessor = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
    m_pGrabber = (Grabber*)m_pGParam->m_SGrabberData.pvoidGrabber;
}
#include <QDebug>
TrackDataStorage::~TrackDataStorage()
{
    m_qmEventStore.lock();
    m_pStoreThread->m_stopRequested = true;
    m_qwcEventStore.wakeAll();
    m_qmEventStore.unlock();
    delete m_pStoreThread;

    qDebug() << "TrackDataStorage";
}

QMutex TrackDataStorage::m_qmEventStore;
QWaitCondition TrackDataStorage::m_qwcEventStore;

void TrackDataStorage::CallBackStore(void* pvoidThis)
{
    TrackDataStorage* pThis = (TrackDataStorage*)pvoidThis;
    QMutexLocker locker(&m_qmEventStore);
    while (!pThis->m_pStoreThread->m_stopRequested)
    {
        m_qwcEventStore.wait(&m_qmEventStore);
        if (!pThis->m_pStoreThread->m_stopRequested)
            pThis->StoreData();
    }
}

void TrackDataStorage::StoreData(void)
{
    QFile data(m_qstrStorePath);
    bool bOpenFile = data.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append);
    if (bOpenFile)
    {
        QTextStream out(&data);

        int iProcMode = m_pImageProcessor->GetProcMode();
        int iTrackMode = m_pImageProcessor->GetTrackMode();
        if ((iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
          || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC))
        {
            sTargetInfo info;
            m_pImageProcessor->GetTargetInfo(info);
            if (info.vectInfoInFrame.size() > 3)
            {
                /// Get Newest Frame Packet (time, point position)
                unsigned long ulFrameSeqTarget = info.vectInfoInFrame[info.vectInfoInFrame.size()-1].ulFrameSeq;
                vector<sFramePacket> vectFramePacket;
                m_pImageProcessor->GetFramePacket(vectFramePacket);
                unsigned long ulFrameSeqLast = vectFramePacket[vectFramePacket.size()-1].ulFrameSeq;
                sFramePacket framePacket = vectFramePacket[vectFramePacket.size()-1 - (ulFrameSeqLast - ulFrameSeqTarget)];

                sMeasureBlob blob = info.vectInfoInFrame[info.vectInfoInFrame.size()-1].blobMeasure;

                QString qstrPacketTime;
                qstrPacketTime.sprintf("%04d-%02d-%02d %2d:%02d:%02d.%03d",
                                       framePacket.stimeFrame.iYear,
                                       framePacket.stimeFrame.iMonth,
                                       framePacket.stimeFrame.iDay,
                                       framePacket.stimeFrame.iHour,
                                       framePacket.stimeFrame.iMinute,
                                       framePacket.stimeFrame.iSecond,
                                       framePacket.stimeFrame.iMillisecond);

                QString qstrLine = QString::number(ulFrameSeqTarget) + " "
                        + qstrPacketTime + " "
                        + QString::number(framePacket.pairfFOVCenterAE.first, 'f', 6) + " "
                        + QString::number(framePacket.pairfFOVCenterAE.second, 'f', 6) + " "
//                        + QString::number(blob.pairfPosAE.first, 'f', 6) + " "
//                        + QString::number(blob.pairfPosAE.second, 'f', 6) + " "
                        + QString::number(blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) + " "
                        + QString::number(m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second) + " "
                        + QString::number(blob.fArea) + " "
                        + QString::number(blob.fArea * blob.fArea / 10 * 1.7396483216) + " "
                        + QString::number(m_pGParam->m_SGrabberData.dExposureTime) + " "
                        + QString::number(1350) + " "                        
                        + QString::number(13.2);

                out << qstrLine << endl;
            }
        }
    }
    data.close();
}

void TrackDataStorage::on_SignalTrackData(void)
{
    if (m_bStore)
    {
        m_qwcEventStore.wakeAll();
    }
}

void TrackDataStorage::SetFileName(QString qstrFileName)
{
    m_qstrStorePath = qstrFileName;
    /// 写入标识头
    QString qstrFile = m_qstrStorePath;
    QString qstrPath = qstrFile.left(qstrFile.lastIndexOf('/'));
    QDir dir;
    if (!dir.exists(qstrPath))
    {
        dir.mkdir(qstrPath);
    }
    QFile data(qstrFile);
    bool bOpenFile = data.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append);
    if (bOpenFile)
    {

        m_pGParam->m_STrackDataStorageData.uiStatusStore = STATUS_OK;
    }
    else
    {
        m_pGParam->m_STrackDataStorageData.uiStatusStore = STATUS_ERROR;
    }
    data.close();
}

QString TrackDataStorage::GetFileName(void)
{
    return m_qstrStorePath;
}

void TrackDataStorage::SetStore(bool bStore)
{
    m_bStore = bStore;
}
