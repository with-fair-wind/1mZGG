#include "Grabber.h"

Grabber::Grabber()
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SGrabberData.pvoidGrabber = (void*)this;
    m_pLog = (MyLog*)m_pGParam->m_SLog.pvoidThis;
    m_bWorking = false;
    m_bGrabbing = false;
    m_bWindowEN = false;
    m_bInited = false;
    pixDepth = 0;
    m_ullGrabSeq = 0;
    m_ullGrabSeqCheck = 0;
    m_dExposureTime = m_pGParam->m_SGrabberData.dExposureTimeInit;
    if(m_pGParam->m_SGrabberData.pausGrabDataCurrent == NULL)
    {
        m_pGParam->m_SGrabberData.pausGrabDataCurrent = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataCurrent, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
    }
    if(m_pGParam->m_SGrabberData.pausGrabDataRotate == NULL)
    {
        m_pGParam->m_SGrabberData.pausGrabDataRotate = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataRotate, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
    }
    m_imgtmp = new unsigned short[6200 * m_pGParam->m_SGrabberData.iFullHeight];
    memset(m_imgtmp, 0, 6200 * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
    m_out = new unsigned short[6200 * m_pGParam->m_SGrabberData.iFullHeight];
    memset(m_out, 0, 6200 * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
    m_Xfer    = NULL;
    m_Buffers = NULL;
    m_Acq     = NULL;
    m_pCameraComm = (CommCamera*)m_pGParam->m_SGrabberData.pvoidComm;

    Init("/home/dps/IniFiles/6060.ccf");

    QSettings* qsetSystemInit = new QSettings("/home/dps/IniFiles/CamWorking.ini", QSettings::IniFormat);
    if (qsetSystemInit)
    {
        /// [CamWorking]
        qsetSystemInit->beginGroup("CamWorking");
        if (qsetSystemInit->contains("Hours"))
        {
            m_fCamWorkingHours = qsetSystemInit->value("Hours").toFloat();
        }
        else
        {
            m_fCamWorkingHours = 0;
        }
        qsetSystemInit->endGroup();
        delete qsetSystemInit;
    }
    else
    {
        m_fCamWorkingHours = 0;
    }

    m_fCamTemp = 0;
    m_ulCountQueryTemp = 0;
    m_bQueryTemp = false;

    m_fFrameInterval = 5000.0;

    m_dTimerInterval = 2500.0;
    m_ptimerActiveGrab = new QTimer(this);
    connect(m_ptimerActiveGrab, SIGNAL(timeout()), this, SLOT(on_SignalTimerActiveGrab()));

    m_ptimer = new QTimer(this);
    connect(m_ptimer, SIGNAL(timeout()), this, SLOT(on_SignalTimeOut()));
    m_ptimer->start(1000);

    m_ptimerCheck = new QTimer(this);
    connect(m_ptimerCheck, SIGNAL(timeout()), this, SLOT(on_SignalTimerCheck()));
    m_ptimerCheck->start(10000);

    m_pQueryThread = new StandardThread(CallBackQuery, (void*)this);
    m_pQueryThread->start(QThread::HighPriority);
}

#include <QDebug>

Grabber::~Grabber()
{
    m_qmEventQuery.lock();
    m_pQueryThread->m_stopRequested = true;
    m_qwcEventQuery.wakeAll();
    m_qmEventQuery.unlock();
    delete m_pQueryThread;
    QSettings* qsetSystemInit = new QSettings("/home/dps/IniFiles/CamWorking.ini", QSettings::IniFormat);
    if (qsetSystemInit)
    {
        /// [CamWorking]
        qsetSystemInit->beginGroup("CamWorking");
        if (qsetSystemInit->contains("Hours"))
        {
            qsetSystemInit->setValue("Hours", QString::number(m_fCamWorkingHours));
        }
        qsetSystemInit->endGroup();
        delete qsetSystemInit;
    }

    QString qstrLog = QStringLiteral("Camera Working Hours: ");
    qstrLog += QString::number(m_fCamWorkingHours, 'f', 4);
    m_pLog->InfoMsg(qstrLog);

    /// 更新相机初始化参数
    m_pGParam->m_SGrabberData.dExposureTimeInit = m_dExposureTime;
    /// 停止采集并关闭相机
    if (m_bWorking)
    {
        StopGrab();	// 停止采集
        if (m_Xfer && *m_Xfer)
            m_Xfer->Destroy();

        // Destroy buffer object
        if (m_Buffers && *m_Buffers)
            m_Buffers->Destroy();

        // Destroy acquisition object
        if (m_Acq && *m_Acq)
            m_Acq->Destroy();

        if (m_Xfer)
            delete m_Xfer;
        if (m_Buffers)
            delete m_Buffers;
        if (m_Acq)
            delete m_Acq;
    }
    /// 释放内存
    QMutexLocker locker(&m_pGParam->m_SGrabberData.qmutexGrab);

    if (m_pGParam->m_SGrabberData.pausGrabDataCurrent)
    {
        delete[] m_pGParam->m_SGrabberData.pausGrabDataCurrent;
    }
    if (m_pGParam->m_SGrabberData.pausGrabDataRotate)
    {
        delete[] m_pGParam->m_SGrabberData.pausGrabDataRotate;
    }
    if (m_imgtmp)   delete [] m_imgtmp;
    if (m_out) delete [] m_out;

    qDebug() << "Grabber";
}

QWaitCondition Grabber::m_qwcEventQuery;// 处理事件
QMutex Grabber::m_qmEventQuery;

void Grabber::XferCallback(SapXferCallbackInfo *pInfo)
{
    Grabber* pThis = (Grabber*)(pInfo->GetContext());
    if (pThis->m_bGrabbing)
    {
        if (pThis->m_pGParam->m_SCommExposureData.bReady)
            pThis->m_pGParam->m_SCommExposureData.bReady = false;
        else
            qDebug() << "****** CommExpData Miss. ******";

        pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab = pThis->m_pGParam->m_SCommExposureData.sexpdispdataRecv;  // use to image storage
        qDebug() << "*** XferCallback";

        QDateTime qdtCut = QDateTime::currentDateTime();
//        qDebug() << "Grab:"
//                << " " << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iHourRecv
//                << ":" << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMinuteRecv
//                << ":" << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iSecondRecv
//                << "." << pThis->m_pGParam->m_SGrabberData.sexpdispdataGrab.iMilliSecondRecv
//                << "*** " << qdtCut.time().hour()
//                << ":" << qdtCut.time().minute()
//                << ":" << qdtCut.time().second()
//                << "." << qdtCut.time().msec();

        pThis->GrabImageCopy(pThis->context.image[0]);

        pThis->m_ullGrabSeq++;

        emit pThis->SignalGrabData();
    }
}

#include <QMessageBox>
void Grabber::Init(QString ccf)
{
    int32_t   acqDeviceNumber = 0;
    char*    acqServerName = "Xtium-CL_MX4_1";
    QString qstrCCFPath = ccf;
    QByteArray qbaCCFPath = qstrCCFPath.toLocal8Bit();
    const char* pcacFileName = qbaCCFPath.constData();
    SapLocation loc(acqServerName, acqDeviceNumber);
    DestroyObjects();
    if (SapManager::GetResourceCount(acqServerName, SapManager::ResourceAcq) > 0)
    {
        m_Acq = new SapAcquisition(loc, pcacFileName);
        m_Buffers = new SapBufferWithTrash(1, m_Acq);
        m_Xfer = new SapAcqToBuf(m_Acq, m_Buffers, XferCallback, this);
    }
    else
    {
        m_bWorking = false;
        m_pGParam->m_SGrabberData.iWorkingStatus = STATUS_ERROR;
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("相机连接无效."));
        return;
    }
    if (!CreateObjects())
    {
        m_bWorking = false;
        m_pGParam->m_SGrabberData.iWorkingStatus = STATUS_ERROR;
        QMessageBox::warning(NULL, QStringLiteral("警告"), QStringLiteral("相机连接无效."));
        return;
    }

    m_Acq->GetParameter(CORACQ_PRM_CROP_HEIGHT,&context.height);
    m_Acq->GetParameter(CORACQ_PRM_CROP_WIDTH,&context.width);
    m_Acq->GetParameter(CORACQ_PRM_OUTPUT_FORMAT,&pixFormat);
    m_Acq->GetParameter(CORACQ_PRM_PIXEL_DEPTH, &context.pixDepth);

    m_Buffers->GetParameter(CORBUFFER_PRM_ADDRESS,&context.image[0]);
    pixDepth = context.pixDepth / ((CORDATA_FORMAT_IS_COLOR(pixFormat)) ? 3 : 1);

    QElapsedTimer t;
    t.start();
    while (t.elapsed()<1000) {
        QCoreApplication::processEvents();
    }

    m_pCameraComm->SerialInit(m_bInited);
    if (m_pCameraComm->GetPortStatus())
    {
        m_bWorking = true;
        m_pGParam->m_SGrabberData.iWorkingStatus = STATUS_OK;
    }
    else
    {
        m_bWorking = false;
        m_pGParam->m_SGrabberData.iWorkingStatus = STATUS_ERROR;
    }

    m_pCameraComm->StartGrab(false);    // stop grabbing firstly

    m_Xfer->Grab();

    m_bInited = true;
}

unsigned int Grabber::ResumeGrab()
{
    ImageProcessor* m_pImageProc = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
    int iProcMode = m_pImageProc->GetProcMode();
    int iTrackMode = m_pImageProc->GetTrackMode();
    bool bTrackProc = m_pImageProc->GetTrackProc();

    if (((iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO)
            || (iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
            || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_MANUAL))
            && bTrackProc)
    {
       double dReadTime = 400.0;
       double dRest = 100.0;
       double dTimerInterval = m_dExposureTime + dReadTime + dRest;
       if (m_dExposureTime <= 500.0)
           m_dTimerInterval = dTimerInterval < 2500.0 ? 2500.0 : dTimerInterval;
       else
           m_dTimerInterval = dTimerInterval < m_fFrameInterval ? m_fFrameInterval : dTimerInterval;

       m_dTimerInterval = m_pGParam->m_SDataProcessorData.bForceRePoint ? m_fFrameInterval : m_dTimerInterval;

       /// Full LEO
       m_pGParam->m_SImageProcessorData.bFullLEO = m_dExposureTime <= 500.0;

       m_pCameraComm->StartGrab(false);
       QThread::msleep(10);

       if (m_ptimerActiveGrab->isActive())
           m_ptimerActiveGrab->stop();
       QThread::msleep(10);

       StartGrabTimer();
    }
    else
    {
        if (m_ptimerActiveGrab->isActive())
            m_ptimerActiveGrab->stop();
        QThread::msleep(10);

        m_pCameraComm->StartGrab(true);
    }


    m_bGrabbing = true;
    m_pGParam->m_SGrabberData.iGrabingStatus = STATUS_OK;

    unsigned int uiImageWidth = m_bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiImageHeight = m_bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    emit SignalStartGrab(uiImageWidth, uiImageHeight);
    emit SignalGrabInit(uiImageWidth, uiImageHeight);

    m_qdtBegin = QDateTime::currentDateTime();

    return STATUS_OK;
}

unsigned int Grabber::PauseGrab()
{
    m_bGrabbing = false;
    if (m_ptimerActiveGrab->isActive())
        m_ptimerActiveGrab->stop();
    QThread::msleep(10);
    m_pCameraComm->StartGrab(false);
    m_pGParam->m_SGrabberData.iGrabingStatus = STATUS_ERROR;

    return STATUS_OK;
}

void Grabber::StopGrab()
{
    if (m_ptimerActiveGrab->isActive())
        m_ptimerActiveGrab->stop();
    QThread::msleep(2);

    m_Xfer->Abort();

    m_bWorking = false;
    m_pGParam->m_SGrabberData.iWorkingStatus = STATUS_ERROR;
    m_bGrabbing = false;
    m_pGParam->m_SGrabberData.iGrabingStatus = STATUS_ERROR;
}

unsigned int Grabber::SetExpTime(QString time)
{
    if (m_pCameraComm->GetPortStatus())
    {
        m_pCameraComm->StartGrab(false);
        QThread::msleep(50 + m_dExposureTime);
        m_pCameraComm->OnSetTimeOfExposure(time);
        m_dExposureTime = time.toDouble();
    }

    return STATUS_OK;
}

unsigned int Grabber::SetGain(QString qstrGain)
{
    if (m_pCameraComm->GetPortStatus())
    {
        m_pCameraComm->StartGrab(false);
        QThread::msleep(50 + m_dExposureTime);
        m_pCameraComm->OnSetGain(qstrGain);
    }

    return STATUS_OK;
}

unsigned int Grabber::SetWindowEN(int line,int start)
{
    if (m_pCameraComm->GetPortStatus())
    {
        m_pCameraComm->StartGrab(false);
        QThread::msleep(50 + m_dExposureTime);
    }

    if (line == m_pGParam->m_SGrabberData.iSubHeight)
    {
        QMutexLocker locker(&m_pGParam->m_SGrabberData.qmutexGrab);

        Init("/home/dps/IniFiles/1024.ccf");

        if (m_imgtmp)   delete [] m_imgtmp;
        m_imgtmp = new unsigned short[6200 * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_imgtmp, 0, 6200 * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short));
        if (m_out) delete [] m_out;
        m_out = new unsigned short[6200 * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_out, 0, 6200 * m_pGParam->m_SGrabberData.iSubHeight * sizeof(unsigned short));

        if (m_pGParam->m_SGrabberData.pausGrabDataCurrent) delete [] m_pGParam->m_SGrabberData.pausGrabDataCurrent;
        m_pGParam->m_SGrabberData.pausGrabDataCurrent = new unsigned short[m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataCurrent, 0, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight* sizeof(unsigned short));

        if (m_pGParam->m_SGrabberData.pausGrabDataRotate) delete [] m_pGParam->m_SGrabberData.pausGrabDataRotate;
        m_pGParam->m_SGrabberData.pausGrabDataRotate = new unsigned short[m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataRotate, 0, m_pGParam->m_SGrabberData.iSubWidth * m_pGParam->m_SGrabberData.iSubHeight* sizeof(unsigned short));

        m_bWindowEN = true;

        if (m_bInited)
        {
            QThread::msleep(2000);
            m_pCameraComm->OnSetWindow(line, start);
            QThread::msleep(2000);
        }

        emit SignalGrabInit(m_pGParam->m_SGrabberData.iSubWidth, m_pGParam->m_SGrabberData.iSubHeight);
    }
    else if (line == m_pGParam->m_SGrabberData.iFullHeight)
    {


        Init("/home/dps/IniFiles/6060.ccf");

        if (m_imgtmp)   delete [] m_imgtmp;
        m_imgtmp = new unsigned short[6200 * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_imgtmp, 0, 6200 * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));
        if (m_out) delete [] m_out;
        m_out = new unsigned short[6200 * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_out, 0, 6200 * m_pGParam->m_SGrabberData.iFullHeight * sizeof(unsigned short));

        if (m_pGParam->m_SGrabberData.pausGrabDataCurrent) delete [] m_pGParam->m_SGrabberData.pausGrabDataCurrent;
        m_pGParam->m_SGrabberData.pausGrabDataCurrent = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataCurrent, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight* sizeof(unsigned short));

        if (m_pGParam->m_SGrabberData.pausGrabDataRotate) delete [] m_pGParam->m_SGrabberData.pausGrabDataRotate;
        m_pGParam->m_SGrabberData.pausGrabDataRotate = new unsigned short[m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight];
        memset(m_pGParam->m_SGrabberData.pausGrabDataRotate, 0, m_pGParam->m_SGrabberData.iFullWidth * m_pGParam->m_SGrabberData.iFullHeight* sizeof(unsigned short));

        m_bWindowEN = false;

        if (m_bInited)
        {
            QThread::msleep(2000);
            m_pCameraComm->OnSetWindow(line, start);
            QThread::msleep(2000);
            qDebug() << "Set Window";
        }

        emit SignalGrabInit(m_pGParam->m_SGrabberData.iFullWidth, m_pGParam->m_SGrabberData.iFullHeight);
    }

    return STATUS_OK;
}

void Grabber::TrainCam()
{
    m_pCameraComm->OnTrainCam();
}

void Grabber::SetFrameInterval(float fFrameInterval)
{
    m_fFrameInterval = fFrameInterval;
    if (m_ptimerActiveGrab->isActive())
    {
        m_ptimerActiveGrab->stop();
        m_ptimerActiveGrab->start((int)m_fFrameInterval);
    }
}

bool Grabber::CreateObjects()
{
    // Create acquisition object
    if (m_Acq && !*m_Acq && !m_Acq->Create())
    {
        DestroyObjects();
        return false;
    }

    // Create buffer object
    if (m_Buffers && !*m_Buffers)
    {
        if( !m_Buffers->Create())
        {
            DestroyObjects();
            return false;
        }
        // Clear all buffers
        m_Buffers->Clear();
    }

    // Create transfer object
    if (m_Xfer && !*m_Xfer)
    {
        if( !m_Xfer->Create())
        {
            DestroyObjects();
            return false;
        }
    }

    if( m_Xfer)
        m_Xfer->SetAutoEmpty( TRUE);

    return true;
}

bool Grabber::DestroyObjects()
{
    // Destroy transfer object
    if (m_Xfer && *m_Xfer)
        m_Xfer->Destroy();

    // Destroy buffer object
    if (m_Buffers && *m_Buffers)
        m_Buffers->Destroy();

    // Destroy acquisition object
    if (m_Acq && *m_Acq)
        m_Acq->Destroy();

    if (m_Xfer)
        delete m_Xfer;
    if (m_Buffers)
        delete m_Buffers;
    if (m_Acq)
        delete m_Acq;

    m_Xfer    = NULL;
    m_Buffers = NULL;
    m_Acq     = NULL;

    return true;
}

void Grabber::GrabImageCopy(void* pausGrabData)
{
    std::chrono::high_resolution_clock::time_point beginTime = std::chrono::high_resolution_clock::now();	///////////////////////////

    unsigned int uiImageHeight = m_bWindowEN ? m_pGParam->m_SGrabberData.iSubHeight : m_pGParam->m_SGrabberData.iFullHeight;
    unsigned int uiImageWidth = m_bWindowEN ? m_pGParam->m_SGrabberData.iSubWidth : m_pGParam->m_SGrabberData.iFullWidth;
    unsigned int uiStartColumn = m_bWindowEN ? (m_pGParam->m_SGrabberData.iSubInFullCenterX - uiImageWidth / 2) + (6200 - m_pGParam->m_SGrabberData.iFullWidth) / 2 + 4 : (6200 - m_pGParam->m_SGrabberData.iFullWidth) / 2 + 4;
    {
        QMutexLocker locker(&m_pGParam->m_SGrabberData.qmutexGrab);

        memcpy(m_imgtmp, pausGrabData, 6200 * uiImageHeight * sizeof(unsigned short));

        unsigned char* paucTemp = (unsigned char*)m_imgtmp;
        unsigned int m = 0;


        int iNumThd = 8;
        for (int i = 0; i < uiImageHeight; i++)
        {
            if (i < uiImageHeight / 2)
            {
                m = 6200 * (2 * i + 1);//注:此处使用image->width而不是image->widthStep
            }
            else
            {
                m = 6200* (2 * (i - (uiImageHeight / 2)));
            }
            int k = i * 6200 * 2;

            if (m_bWindowEN)
            {
                #pragma omp parallel for num_threads(iNumThd)
                for (int j = (6200/2-uiImageWidth/2)*2; j < (6200/2-uiImageWidth/2)*2+uiImageWidth*2; j++)
                {
                    if (j % 2 == 1)
                    {
                        m_out[m + j / 2] = (paucTemp[k + j - 1] << 8) | paucTemp[k + j];
                    }
                }
            }
            else
            {
                #pragma omp parallel for num_threads(iNumThd)
                for (int j = 0; j < 6200 * 2; j++)
                {
                    if (j % 2 == 1)
                    {
                        m_out[m + j / 2] = (paucTemp[k + j - 1] << 8) | paucTemp[k + j];
                    }
                }
            }
        }

        #pragma omp parallel for num_threads(iNumThd)
        for (int i = 0; i < uiImageHeight; i++)
        {
            memcpy(m_pGParam->m_SGrabberData.pausGrabDataCurrent + i * uiImageWidth, m_out + i * 6200 + uiStartColumn, uiImageWidth * sizeof(unsigned short));
        }
    }

    std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();	///////////////////////////
    std::chrono::milliseconds timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime);	///////////////////////////
    QDateTime qdtCut = QDateTime::currentDateTime();
//    qDebug() << "***UnPacket: " << timeInterval.count() << "ms***"
//            << " " << m_pGParam->m_SGrabberData.sexpdispdataGrab.iHourRecv
//            << ":" << m_pGParam->m_SGrabberData.sexpdispdataGrab.iMinuteRecv
//            << ":" << m_pGParam->m_SGrabberData.sexpdispdataGrab.iSecondRecv
//            << "." << m_pGParam->m_SGrabberData.sexpdispdataGrab.iMilliSecondRecv
//            << "*** " << qdtCut.time().hour()
//            << ":" << qdtCut.time().minute()
//            << ":" << qdtCut.time().second()
//            << "." << qdtCut.time().msec();
}

void Grabber::on_SignalTimeOut()
{
    if (m_bGrabbing)
    {
        m_qdtEnd = QDateTime::currentDateTime();
        m_fCamWorkingHours += (float)m_qdtBegin.msecsTo(m_qdtEnd) / 3600000.0;
        m_qdtBegin = m_qdtEnd;
    }

    m_qwcEventQuery.wakeAll();
}

void Grabber::on_SignalTimerCheck()
{
    if (m_bGrabbing)
    {
        if (m_ullGrabSeqCheck != m_ullGrabSeq)
        {
            m_pGParam->m_SGrabberData.iRecvStatus = STATUS_OK;
        }
        else
        {
            m_pGParam->m_SGrabberData.iRecvStatus = STATUS_ERROR;
        }
        m_ullGrabSeqCheck = m_ullGrabSeq;
    }
}

void Grabber::on_SignalTimerActiveGrab()
{
    qDebug() << "%%% Grab Interval:" << ClockOff() << "ms";
    GrabSingleFrame();
    ClockOn();
}

void Grabber::CallBackQuery(void *pvoidThis)
{
    Grabber* pThis = (Grabber*)pvoidThis;

    QMutexLocker locker(&m_qmEventQuery);
    while (!pThis->m_pQueryThread->m_stopRequested)
    {
        m_qwcEventQuery.wait(&m_qmEventQuery);
        if (!pThis->m_pQueryThread->m_stopRequested)
        {
            if (pThis->m_ulCountQueryTemp++ > 20 || pThis->m_bQueryTemp)
            {
                pThis->m_bQueryTemp = true;
                pThis->m_pCameraComm->OnQueryTemp(pThis->m_fCamTemp);
            }
        }
    }
}

void Grabber::GrabSingleFrame()
{
    m_pCameraComm->StartGrab(true);
    QThread::msleep(5);
    m_pCameraComm->StartGrab(false);
}

void Grabber::StartGrabTimer()
{
    GrabSingleFrame();
    m_ptimerActiveGrab->start(m_dTimerInterval);
    ClockOn();
}

void Grabber::ClockOn()
{
    m_beginTime = std::chrono::high_resolution_clock::now();
}

int Grabber::ClockOff()
{
    m_endTime = std::chrono::high_resolution_clock::now();	///////////////////////////
    return (std::chrono::duration_cast<std::chrono::milliseconds>(m_endTime - m_beginTime)).count();	///////////////////////////
}

