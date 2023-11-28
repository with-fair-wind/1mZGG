#include "CommMasterControl.h"

CommMasterControl::CommMasterControl(void)
{
    /// 成员变量
    m_pGParam = GlobalParameter::GetInstance();
    m_pSGlobalData = &m_pGParam->m_SCommMasterControlData;
    m_qstrPort = "/dev/ttyUSB2S";
    m_bPortIsOpen = false;
    m_iRecvFrameFrequency = 25;
    m_iRecvFramePerSec = 0;
    m_ucRecvTimeMark = 0;
    m_iRecvTimeMarkNoChange = 0;
    m_iRecvBytes = 30;
    m_cRecvFrameBegin = 0x7E;
    m_cRecvFrameEnd = 0xE7;
    m_pacRecvBuffer = new char[m_iRecvBytes];
    memset(m_pacRecvBuffer, 0, m_iRecvBytes);
    m_paucRecvBuffer = new unsigned char[m_iRecvBytes];
    memset(m_paucRecvBuffer, 0, m_iRecvBytes);
    m_iSendFrameFrequency = 25;
    m_iSendFramePerSec = 0;
    m_ucSendTimeMark = 0;
    m_iSendBytes = 28;
    m_bSendStatus = false;
    m_pacSendBuffer = new char[m_iSendBytes];
    memset(m_pacSendBuffer, 0, m_iSendBytes);
    m_paucSendBuffer = new unsigned char[m_iSendBytes];
    memset(m_paucSendBuffer, 0, m_iSendBytes);

    m_fExpTime = 0;
    m_uiMode1 = 9999;
    m_uiMode2 = 9999;
    m_bTrack = false;
    m_bImageSave = false;
    m_uiTargetID = 888888;
    m_uiTaskID = 999999;
    m_uiHourStart = 8;
    m_uiMinuteStart = 8;
    m_uiSecondStart = 0;
    m_uiHourEnd = 9;
    m_uiMinuteEnd = 9;
    m_uiSecondEnd = 0;
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
CommMasterControl::~CommMasterControl(void)
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

    qDebug() << "CommMasterControl";
}

void CommMasterControl::on_SignalInitPort()
{
    m_serialPort = new QSerialPort;
    InitPort(m_pGParam->m_SCommNetSettings.qstrPortMasterControl);
    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(receiveInfo()));
}

bool CommMasterControl::InitPort(QString qstrPort)
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

void CommMasterControl::receiveInfo()
{
    if (!m_bPortIsOpen)
    {
        InitPort(m_pGParam->m_SCommNetSettings.qstrPortDisplay);
    }
    else
    {
        QByteArray info = m_serialPort->readAll();
        if (info[0] == m_cRecvFrameBegin && info[m_iRecvBytes-1] == m_cRecvFrameEnd)
        {
            memcpy(m_pacRecvBuffer, info.data(), m_iRecvBytes);
            ProcessRecvData();
        }
    }
}

void CommMasterControl::sendInfo()
{
//    if (m_bPortIsOpen)
    {
        ProcessSendData(0);
        m_serialPort->clear(QSerialPort::Output);
        m_serialPort->write(m_pacSendBuffer, m_iSendBytes);
        ProcessSendData(1);
    }
}

void CommMasterControl::ProcessRecvData(void)
{
    m_iRecvFramePerSec++;	// 接收帧数自加
    unsigned char* paucRecvBuffer = (unsigned char*)m_pacRecvBuffer;	// char数组转换为unsigned char数组
    {
        m_pSGlobalData->iCommErrorRecvTimeMark = STATUS_OK;
        /// 解码
        float fExpTime = ((float)m_paucRecvBuffer[5] * 65536.0 + (float)m_paucRecvBuffer[4] * 256.0 + (float)m_paucRecvBuffer[3]) / 100.0;
        unsigned int uiMode1 = m_paucRecvBuffer[7];
        unsigned int uiMode2 = m_paucRecvBuffer[8];
        int iIndex_ComboTrackMode = -1;
        bool bSearch = false;
        switch (uiMode1)
        {
        case 0x01:
            if (uiMode2 == 0x00)
            {
                iIndex_ComboTrackMode = 0;
            }
            else
            {
                iIndex_ComboTrackMode = 1;
            }
            break;
        case 0x02:
            iIndex_ComboTrackMode = 0;
            break;
        case 0x03:
            iIndex_ComboTrackMode = 2;
            break;
        case 0x04:
            if (uiMode2 == 0x00)
            {
                iIndex_ComboTrackMode = 0;
            }
            else
            {
                iIndex_ComboTrackMode = 1;
            }
            break;
        case 0x05:
            iIndex_ComboTrackMode = 3;
            break;
        default:
            break;
        }
        bool bTrack = m_paucRecvBuffer[9] == 0xFF;
//        bTrack = !(uiMode1 == 0x00);
        bool bImageSave = m_paucRecvBuffer[10] == 0xFF;
        unsigned int uiTargetID = (unsigned int)m_paucRecvBuffer[11] + (unsigned int)m_paucRecvBuffer[12] * 256 + (unsigned int)m_paucRecvBuffer[13] * 65536;
        unsigned int uiTaskID = (unsigned int)m_paucRecvBuffer[14] + (unsigned int)m_paucRecvBuffer[15] * 256 + (unsigned int)m_paucRecvBuffer[16] * 65536;
        unsigned int uiHourStart = m_paucRecvBuffer[17];
        unsigned int uiMinuteStart = m_paucRecvBuffer[18];
        unsigned int uiSecondStart = m_paucRecvBuffer[19];
        unsigned int uiHourEnd = m_paucRecvBuffer[20];
        unsigned int uiMinuteEnd = m_paucRecvBuffer[21];
        unsigned int uiSecondEnd = m_paucRecvBuffer[22];

//        if (m_fExpTime != fExpTime)
        {
            m_fExpTime = fExpTime;
//            emit SignalMCSetExp(m_fExpTime);
        }
//        if (m_uiMode1 != uiMode1 || m_uiMode2 != uiMode2)
        {
            m_uiMode1 = uiMode1;
            m_uiMode2 = uiMode2;
//            emit SignalMCSetTrackMode(iIndex_ComboTrackMode);
        }
//        if (m_bTrack != bTrack)
        {
            m_bTrack = bTrack;
//            emit SignalMCSetTrack(m_bTrack);
        }
//        if (m_bImageSave != bImageSave)
        {
            m_bImageSave = bImageSave;
//            emit SignalMCSetImageSave(m_bImageSave);
        }
        if (m_uiTargetID != uiTargetID
                || m_uiTaskID != uiTaskID
                || m_uiHourStart != uiHourStart
                || m_uiMinuteStart != uiMinuteStart
                || m_uiSecondStart != uiSecondStart
                || m_uiHourEnd != uiHourEnd
                || m_uiMinuteEnd != uiMinuteEnd
                || m_uiSecondEnd != uiSecondEnd)
        {
            m_uiTargetID = uiTargetID;
            m_uiTaskID = uiTaskID;
            m_uiHourStart = uiHourStart;
            m_uiMinuteStart = uiMinuteStart;
            m_uiSecondStart = uiSecondStart;
            m_uiHourEnd = uiHourEnd;
            m_uiMinuteEnd = uiMinuteEnd;
            m_uiSecondEnd = uiSecondEnd;
            m_pGParam->m_SNetMasterControlData.bSearch = bSearch;
            m_pGParam->m_SNetMasterControlData.qstrTaskID.sprintf("%06d", uiTaskID);
            m_pGParam->m_SNetMasterControlData.qstrTargetID.sprintf("%06d", uiTargetID);
            QDateTime qdtCurrent = QDateTime::currentDateTime();
            unsigned int uiCurrentHour = qdtCurrent.time().hour();
            unsigned int uiYear, uiMonth, uiDay, uiYearNew, uiMonthNew, uiDayNew;
            uiYear = qdtCurrent.date().year();
            uiMonth = qdtCurrent.date().month();
            uiDay = qdtCurrent.date().day();
//            if (abs(uiCurrentHour-m_uiHourStart)>12)
//            {
//                DateMinus1(uiYear, uiMonth, uiDay, uiYearNew, uiMonthNew, uiDayNew);
//            }
//            else
            {
                uiYearNew = uiYear;
                uiMonthNew = uiMonth;
                uiDayNew = uiDay;
            }
            m_pGParam->m_SNetMasterControlData.qstrStartTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                                                     uiYearNew,
                                                                     uiMonthNew,
                                                                     uiDayNew,
                                                                     m_uiHourStart,
                                                                     m_uiMinuteStart,
                                                                     m_uiSecondStart);
            m_pGParam->m_SNetMasterControlData.qstrEndTime.sprintf("%04d%02d%02d%02d%02d%02d",
                                                                     uiYearNew,
                                                                     uiMonthNew,
                                                                     uiDayNew,
                                                                     m_uiHourEnd,
                                                                     m_uiMinuteEnd,
                                                                     m_uiSecondEnd);
            m_pGParam->m_SNetMasterControlData.bTaskChanged = true;
//            emit SignalMCSetTaskChanged();
        }
        emit SignalMCSet(m_fExpTime, iIndex_ComboTrackMode, m_bTrack, m_bImageSave);
    }
    memcpy(m_paucRecvBuffer, paucRecvBuffer, m_iRecvBytes);	// 复制接收缓存
    /// 数据写入txt
    if (m_pSGlobalData->bSaveData)
    {
        QString qstrFile = m_pGParam->m_SPath.qstrDataStorePath + "/Save_CommData/RecvData_Display.txt";
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

void CommMasterControl::ProcessSendData(int iProcMode)
{
    if (iProcMode == 0)
    {
        m_iSendFramePerSec++;	// 发送帧数自加
        memset(m_pacSendBuffer, 0, m_iSendBytes);
        {
            QMutexLocker locker(&m_pSGlobalData->qmutexSend);
            /// 编码
            m_pacSendBuffer[0] = 0x7E;

            ImageProcessor* pImageProc = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
            int iTrackMode = pImageProc->GetTrackMode();
            int iProcMode = pImageProc->GetProcMode();
            if ((iTrackMode == TRACK_LEO && iProcMode == PROC_DIRECT)
                    || (iTrackMode == TRACK_SC && iProcMode == PROC_DIRECT))
            {
                sTargetInfo info;
                pImageProc->GetTargetInfo(info);
                /// Get Newest Frame Packet (time, point position)
                if ((int)info.vectInfoInFrame.size()-1 >= 0)
                {
                    unsigned long ulFrameSeqTarget = info.vectInfoInFrame[info.vectInfoInFrame.size()-1].ulFrameSeq;
                    vector<sFramePacket> vectFramePacket;
                    pImageProc->GetFramePacket(vectFramePacket);
                    if (info.vectInfoInFrame.size() >= 4 && info.bLiving && info.fValid >= 0.8 && vectFramePacket.size() > 0)
                    {
                        unsigned long ulFrameSeqLast = vectFramePacket[vectFramePacket.size()-1].ulFrameSeq;
                        sFramePacket framePacket = vectFramePacket[vectFramePacket.size()-1 - (ulFrameSeqLast - ulFrameSeqTarget)];

                        m_pacSendBuffer[1] = m_pGParam->m_SCommServoData.bDistValid ? 0x01 : 0x00;
                        m_pacSendBuffer[1] = m_pGParam->m_SCommServoData.bSpdValid ? m_pacSendBuffer[1]|0x10 : m_pacSendBuffer[1];
                        int iSize = info.vectInfoInFrame.size();
                        sMeasureBlob blob = info.vectInfoInFrame[iSize-1].blobMeasure;
                        float fElevation = framePacket.pairfFOVCenterAE.second;
                        float fDistX = (blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX) * m_pGParam->m_SOpticData.fPixelScale * 3600 * (1.0/cos(fElevation*3.1415926/180.0));
                        float fDistY = (m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second) * m_pGParam->m_SOpticData.fPixelScale * 3600;
                        m_pacSendBuffer[2] = (unsigned int)(abs(fDistX)  * 10) & 0xff;
                        m_pacSendBuffer[3] = ((unsigned int)(abs(fDistX) * 10) >> 8) & 0xff;
                        m_pacSendBuffer[3] = fDistX >= 0 ? m_pacSendBuffer[3] : m_pacSendBuffer[3] | 0x80;
                        m_pacSendBuffer[4] = (unsigned int)(abs(fDistY) * 10) & 0xff;
                        m_pacSendBuffer[5] = ((unsigned int)(abs(fDistY) * 10) >> 8) & 0xff;
                        m_pacSendBuffer[5] = fDistY >= 0 ? m_pacSendBuffer[5] : m_pacSendBuffer[5] | 0x80;

                        int iYear = framePacket.stimeFrame.iYear - 2000;
                        int iMonth = framePacket.stimeFrame.iMonth;
                        int iDay = framePacket.stimeFrame.iDay;
                        int iHour = framePacket.stimeFrame.iHour;
                        int iMinute = framePacket.stimeFrame.iMinute;
                        int iSecond = framePacket.stimeFrame.iSecond;
                        int iMilliSecond = framePacket.stimeFrame.iMillisecond;
                        m_pacSendBuffer[6] = ((iYear/10)<<4) + ((iYear%10));
                        m_pacSendBuffer[7] = ((iMonth/10)<<4) + ((iMonth%10));
                        m_pacSendBuffer[8] = ((iDay/10)<<4) + ((iDay%10));
                        m_pacSendBuffer[9] = ((iHour/10)<<4) + (iHour%10);
                        m_pacSendBuffer[10] = ((iMinute/10)<<4) + (iMinute%10);
                        m_pacSendBuffer[11] = ((iSecond/10)<<4) + (iSecond%10);
                        m_pacSendBuffer[12] = (iMilliSecond*10) & 0xff;
                        m_pacSendBuffer[13] = ((iMilliSecond*10)>>8) & 0xff;

                        unsigned long ulAzimuth = (unsigned long)(framePacket.pairfFOVCenterAE.first / 360.0 * pow(2.0, 29.0));
                        unsigned long ulElevation = (unsigned long)(framePacket.pairfFOVCenterAE.second / 360.0 * pow(2.0, 29.0));
                        m_pacSendBuffer[14] = ulAzimuth & 0xff;
                        m_pacSendBuffer[15] = (ulAzimuth >> 8) & 0xff;
                        m_pacSendBuffer[16] = (ulAzimuth >> 16) & 0xff;
                        m_pacSendBuffer[17] = (ulAzimuth >> 24) & 0xff;
                        m_pacSendBuffer[18] = ulElevation & 0xff;
                        m_pacSendBuffer[19] = (ulElevation >> 8) & 0xff;
                        m_pacSendBuffer[20] = (ulElevation >> 16) & 0xff;
                        m_pacSendBuffer[21] = (ulElevation >> 24) & 0xff;
                    }
                }
            }
            m_pacSendBuffer[27] = 0xE7;
        }
    }
    else if (iProcMode == 1)
    {
        memcpy(m_paucSendBuffer, (unsigned char*)m_pacSendBuffer, m_iSendBytes);	// 复制发送缓存
        m_pSGlobalData->iCommErrorSend = m_bSendStatus ? STATUS_OK : STATUS_ERROR;
        /// 数据写入txt
        if (m_pSGlobalData->bSaveData)
        {
            QString qstrFile = m_pGParam->m_SPath.qstrDataStorePath + "/Save_CommData/SendData_Display.txt";
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
                QString qstrAdd, qstrSend;
                for (int i = 0; i < m_iSendBytes; i++)
                {
                    qstrAdd.asprintf("%02X ", m_paucSendBuffer[i]);
                    qstrSend += qstrAdd;
                }
                out << qstrSend << endl;
            }
            data.close();
        }
        QString qstrAdd, qstrSend;
        for (int i = 0; i < m_iSendBytes; i++)
        {
            qstrAdd.sprintf("%02X ", m_paucSendBuffer[i]);
            qstrSend += qstrAdd;
        }
//        qDebug() << qstrSend;
    }
}

void CommMasterControl::ProcessRecvCheck(void)
{
    m_pSGlobalData->iCommErrorRecvLost = float(m_iRecvFramePerSec) < float(m_iRecvFrameFrequency) / 2.0f ? STATUS_ERROR : STATUS_OK;	// 接收丢失帧数超限
    qDebug() << "SecNum: " << m_iRecvFramePerSec;
    m_pSGlobalData->iRecvFramePerSec = m_iRecvFramePerSec;
    m_iRecvFramePerSec = 0;
    m_pSGlobalData->iSendFramePerSec = m_iSendFramePerSec;
    m_iSendFramePerSec = 0;
    if (m_pSGlobalData->iCommErrorRecvLost == STATUS_ERROR)
    {
        m_pLog->CriticalMsg(QStringLiteral("显示数据端口接收帧数丢失超限."));
    }
}

void CommMasterControl::DateMinus1(unsigned int uiYear, unsigned int uiMonth, unsigned int uiDay, unsigned int &uiYearNew, unsigned int &uiMonthNew, unsigned int &uiDayNew)
{
    uiDayNew = uiDay - 1;
    if (uiDayNew < 1)
    {
        switch (uiMonth)
        {
        case 1:
            uiYearNew = uiYear - 1;
            uiMonthNew = 12;
            uiDayNew = 31;
        break;
        case 2:
            uiYearNew = uiYear;
            uiMonthNew = 1;
            uiDayNew = 31;
        break;
        case 3:
            uiYearNew = uiYear;
            uiMonthNew = 2;
            uiDayNew = uiYear % 4 == 0 ? 29 : 28;
        break;
        case 4:
            uiYearNew = uiYear;
            uiMonthNew = 3;
            uiDayNew = 31;
        break;
        case 5:
            uiYearNew = uiYear;
            uiMonthNew = 4;
            uiDayNew = 30;
        break;
        case 6:
            uiYearNew = uiYear;
            uiMonthNew = 5;
            uiDayNew = 31;
        break;
        case 7:
            uiYearNew = uiYear;
            uiMonthNew = 6;
            uiDayNew = 30;
        break;
        case 8:
            uiYearNew = uiYear;
            uiMonthNew = 7;
            uiDayNew = 31;
        break;
        case 9:
            uiYearNew = uiYear;
            uiMonthNew = 8;
            uiDayNew = 31;
        break;
        case 10:
            uiYearNew = uiYear;
            uiMonthNew = 9;
            uiDayNew = 30;
        break;
        case 11:
            uiYearNew = uiYear;
            uiMonthNew = 10;
            uiDayNew = 31;
        break;
        case 12:
            uiYearNew = uiYear;
            uiMonthNew = 11;
            uiDayNew = 30;
        break;
        default:
        break;
        }
    }
}
