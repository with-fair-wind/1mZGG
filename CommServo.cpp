#include "CommServo.h"

CommServo::CommServo(void)
{
    /// 成员变量
    m_pGParam = GlobalParameter::GetInstance();
    m_pSGlobalData = &m_pGParam->m_SCommServoData;
    m_qstrPort = "/dev/ttyUSB3S";
    m_bPortIsOpen = false;
    m_iRecvFrameFrequency = 0;
    m_iRecvFramePerSec = 0;
    m_ucRecvTimeMark = 0;
    m_iRecvTimeMarkNoChange = 0;
    m_iRecvBytes = 20;
    m_cRecvFrameBegin = 0x7E;
    m_cRecvFrameEnd = 0xE7;
    m_pacRecvBuffer = new char[m_iRecvBytes];
    memset(m_pacRecvBuffer, 0, m_iRecvBytes);
    m_paucRecvBuffer = new unsigned char[m_iRecvBytes];
    memset(m_paucRecvBuffer, 0, m_iRecvBytes);
    m_iSendFrameFrequency = 25;
    m_iSendFramePerSec = 0;
    m_ucSendTimeMark = 0;
    m_iSendBytes = 14;
    m_bSendStatus = false;
    m_pacSendBuffer = new char[m_iSendBytes];
    memset(m_pacSendBuffer, 0, m_iSendBytes);
    m_paucSendBuffer = new unsigned char[m_iSendBytes];
    memset(m_paucSendBuffer, 0, m_iSendBytes);
    m_pImageProc = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
    /// 外部变量
    m_pSGlobalData->iRecvBytes = m_iRecvBytes;
    m_pSGlobalData->paucRecvBuffer = m_paucRecvBuffer;
    m_pSGlobalData->iSendBytes = m_iSendBytes;
    m_pSGlobalData->paucSendBuffer = m_paucSendBuffer;
    m_pLog = (MyLog*)m_pGParam->m_SLog.pvoidThis;

    m_qthdRun = new QThread;
    moveToThread(m_qthdRun);
    m_qthdRun->start();

    connect(this, SIGNAL(SignalInitPort()), this, SLOT(on_SignalInitPort()));
    emit SignalInitPort();
}

#include <QDebug>
CommServo::~CommServo(void)
{
    if (m_bPortIsOpen)
    {
        m_bPortIsOpen = false;
//        m_serialPort->close();
    }

    delete[] m_pacRecvBuffer;
    delete[] m_paucRecvBuffer;
    delete[] m_pacSendBuffer;
    delete[] m_paucSendBuffer;

    qDebug() << "CommServo";
}

void CommServo::on_SignalInitPort()
{
    m_serialPort = new QSerialPort;
    InitPort(m_pGParam->m_SCommNetSettings.qstrPortServo);
}

bool CommServo::InitPort(QString qstrPort)
{
    m_qstrPort = qstrPort;
    m_serialPort->setPortName(qstrPort);
    if (m_serialPort->isOpen())
    {
        m_serialPort->clear();
        m_serialPort->close();
    }
    m_bPortIsOpen = m_serialPort->open(QIODevice::ReadWrite);
    m_bPortIsOpen &= m_serialPort->setBaudRate(230400);
    m_bPortIsOpen &= m_serialPort->setDataBits(QSerialPort::Data8);
    m_bPortIsOpen &= m_serialPort->setParity(QSerialPort::NoParity);
    m_bPortIsOpen &= m_serialPort->setStopBits(QSerialPort::OneStop);
    m_bPortIsOpen &= m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    m_pSGlobalData->iStatusPort = m_bPortIsOpen ? STATUS_OK : STATUS_ERROR;

    return m_bPortIsOpen;
}

void CommServo::sendInfo()
{
    if (m_bPortIsOpen)
    {
        ProcessSendData(0);
        m_serialPort->clear(QSerialPort::Output);
        m_serialPort->write(m_pacSendBuffer, m_iSendBytes);
        ProcessSendData(1);
    }
}

void CommServo::ProcessRecvData(void)
{
    m_iRecvFramePerSec++;	// 接收帧数自加
    unsigned char* paucRecvBuffer = (unsigned char*)m_pacRecvBuffer;	// char数组转换为unsigned char数组
//	m_iRecvTimeMarkNoChange = paucRecvBuffer[17] == m_ucRecvTimeMark ? m_iRecvTimeMarkNoChange + 1 : 0;	// 判断时标是否未变
//	if (m_iRecvTimeMarkNoChange < 10)	// 判断时标连续未变是否超限
    {
        m_pSGlobalData->iCommErrorRecvTimeMark = STATUS_OK;
        /// 解码
        {
            QMutexLocker locker(&m_pSGlobalData->qmutexRecv);
            /// 解码

        }
    }
    //else // 时标连续未变超限
    //{
    //	m_pSGlobalData->iCommErrorRecvTimeMark = STATUS_ERROR;
    //}
    {
        QMutexLocker locker(&m_pSGlobalData->qmutexRecv);
        memcpy(m_paucRecvBuffer, paucRecvBuffer, m_iRecvBytes);	// 复制接收缓存 CommMaster访问m_paucRecvBuffer互斥
    }
    /// 数据写入txt
    if (m_pSGlobalData->bSaveData)
    {
        QString qstrFile = m_pGParam->m_SPath.qstrDataStorePath + "/Save_CommData/RecvData_Exposure.txt";
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
            QTextStream out(&data);
            QDateTime qdatetimeCurrent = QDateTime::currentDateTime();
            QString qstrCurrentTime = qdatetimeCurrent.toString("yyyy-MM-dd hh:mm:ss.zzz");
            out << qstrCurrentTime << endl;
            QString qstrAdd, qstrRecv;
            for (int i = 0; i < m_iRecvBytes; i++)
            {
                qstrAdd.asprintf("%02X ", m_paucRecvBuffer[i]);
                qstrRecv += qstrAdd;
            }
            out << qstrRecv << endl;
        }
        data.close();
    }
}

void CommServo::ProcessSendData(int iProcMode)
{
    if (iProcMode == 0)
    {
        m_iSendFramePerSec++;	// 发送帧数自加
        memset(m_pacSendBuffer, 0, m_iSendBytes);
        /// 编码
        m_pacSendBuffer[0] = 0x7E;
        int iTrackMode = m_pImageProc->GetTrackMode();
        int iProcMode = m_pImageProc->GetProcMode();
        if ((iTrackMode == TRACK_LEO && iProcMode == PROC_DIRECT)
                || (iTrackMode == TRACK_SC && iProcMode == PROC_DIRECT))
        {
            sTargetInfo targetInfo;
            m_pImageProc->GetTargetInfo(targetInfo);
            if (targetInfo.vectInfoInFrame.size() >= 4 && targetInfo.bLiving && targetInfo.fValid >= 0.8)
            {
                m_pacSendBuffer[1] = m_pSGlobalData->bDistValid ? 0x01 : 0x00;
                m_pacSendBuffer[1] = m_pSGlobalData->bSpdValid ? m_pacSendBuffer[1]|0x10 : m_pacSendBuffer[1];
                int iSize = targetInfo.vectInfoInFrame.size();
                sMeasureBlob blob = targetInfo.vectInfoInFrame[iSize-1].blobMeasure;
                float fElevation = blob.pairfPosAE.second;
                float fDistX = (blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale * 3600 * (1.0/cos(fElevation*3.1415926/180.0));
                float fDistY = (m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale * 3600 ;
                m_pacSendBuffer[2] = (unsigned int)(abs(fDistX) * 10) & 0xff;
                m_pacSendBuffer[3] = ((unsigned int)(abs(fDistX) * 10) >> 8) & 0xff;
                m_pacSendBuffer[3] = fDistX >= 0 ? m_pacSendBuffer[3] : m_pacSendBuffer[3] | 0x80;
                m_pacSendBuffer[4] = (unsigned int)(abs(fDistY) * 10) & 0xff;
                m_pacSendBuffer[5] = ((unsigned int)(abs(fDistY) * 10) >> 8) & 0xff;
                m_pacSendBuffer[5] = fDistY >= 0 ? m_pacSendBuffer[5] : m_pacSendBuffer[5] | 0x80;
                float fSpdX = targetInfo.pairfPredSpdAE.first * 3600;
                float fSpdY = targetInfo.pairfPredSpdAE.second * 3600;
                m_pacSendBuffer[6] = (unsigned int)abs(fSpdX) & 0xff;
                m_pacSendBuffer[7] = ((unsigned int)abs(fSpdX) >> 8) & 0xff;
                m_pacSendBuffer[8] = ((unsigned int)abs(fSpdX) >> 16) & 0xff;
                m_pacSendBuffer[8] = fSpdX > 0 ? m_pacSendBuffer[8] : m_pacSendBuffer[8] | 0x80;
                m_pacSendBuffer[9] = (unsigned int)abs(fSpdY) & 0xff;
                m_pacSendBuffer[10] = ((unsigned int)abs(fSpdY) >> 8) & 0xff;
                m_pacSendBuffer[11] = ((unsigned int)abs(fSpdY) >> 16) & 0xff;
                m_pacSendBuffer[11] = fSpdY > 0 ? m_pacSendBuffer[11] : m_pacSendBuffer[11] | 0x80;
                m_pacSendBuffer[12] = 0x19;
            }
        }
        m_pacSendBuffer[13] = 0xE7;
    }
    else if (iProcMode == 1)
    {
        memcpy(m_paucSendBuffer, (unsigned char*)m_pacSendBuffer, m_iSendBytes);	// 复制发送缓存
        m_pSGlobalData->iCommErrorSend = m_bSendStatus ? STATUS_OK : STATUS_ERROR;
        /// 数据写入txt
        if (m_pSGlobalData->bSaveData)
        {
            QString qstrFile = "/home/dps/SendData_Servo";
            QString qstrPath = qstrFile.left(qstrFile.lastIndexOf('/'));
//            QDir dir;
//            if (!dir.exists(qstrPath))
//            {
//                dir.mkdir(qstrPath);
//            }
            QFile data(qstrFile);
            bool bOpenFile = data.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append);
            if (bOpenFile)
            {
                QTextStream out(&data);
                QDateTime qdatetimeCurrent = QDateTime::currentDateTime();
                QString qstrCurrentTime = qdatetimeCurrent.toString("yyyy-MM-dd hh:mm:ss.zzz");
                out << qstrCurrentTime << endl;
                QString qstrAdd, qstrSend;
                for (int i = 0; i < m_iSendBytes; i++)
                {
                    qstrAdd.sprintf("%02X ", m_paucSendBuffer[i]);
                    qstrSend += qstrAdd;
                }
                out << qstrSend << endl;
            }
            data.close();
        }
    }
}

void CommServo::ProcessRecvCheck(void)
{
    m_pSGlobalData->iCommErrorRecvLost = float(m_iRecvFramePerSec) < float(m_iRecvFrameFrequency) / 2.0f ? STATUS_ERROR : STATUS_OK;	// 接收丢失帧数超限
    m_pSGlobalData->iRecvFramePerSec = m_iRecvFramePerSec;
    m_iRecvFramePerSec = 0;
    m_pSGlobalData->iSendFramePerSec = m_iSendFramePerSec;
    m_iSendFramePerSec = 0;
    if (m_pSGlobalData->iCommErrorRecvLost == STATUS_ERROR)
    {
        m_pLog->CriticalMsg(QStringLiteral("曝光数据端口接收帧数丢失超限."));
    }
}

