#include "CommDisplay.h"

CommDisplay::CommDisplay(void)
{
    /// 成员变量
    m_pGParam = GlobalParameter::GetInstance();
    m_pSGlobalData = &m_pGParam->m_SCommDisplayData;
    m_qstrPort = "/dev/ttyUSB0S";
    m_bPortIsOpen = false;
    m_iRecvFrameFrequency = 25;
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
    m_iSendBytes = 9;
    m_bSendStatus = false;
    m_pacSendBuffer = new char[m_iSendBytes];
    memset(m_pacSendBuffer, 0, m_iSendBytes);
    m_paucSendBuffer = new unsigned char[m_iSendBytes];
    memset(m_paucSendBuffer, 0, m_iSendBytes);
    /// 外部变量
    m_pSGlobalData->iRecvBytes = m_iRecvBytes;
    m_pSGlobalData->paucRecvBuffer = m_paucRecvBuffer;
    m_pSGlobalData->iSendBytes = m_iSendBytes;
    m_pSGlobalData->paucSendBuffer = m_paucSendBuffer;
    m_pLog = (MyLog*)m_pGParam->m_SLog.pvoidThis;

    m_qthdRun = new QThread;
    moveToThread(m_qthdRun);
    m_qthdRun->start();

    m_pqTimer = new QTimer;
    connect(m_pqTimer, SIGNAL(timeout()), this, SLOT(on_SignalTimerRecvCheck()));
    m_pqTimer->start(1000);

    connect(this, SIGNAL(SignalInitPort()), this, SLOT(on_SignalInitPort()));
    emit SignalInitPort();
}

#include <QDebug>
CommDisplay::~CommDisplay(void)
{
    if (m_bPortIsOpen)
    {
        m_bPortIsOpen = false;
//        m_serialPort->close();
    }

    delete m_pqTimer;
    delete[] m_pacRecvBuffer;
    delete[] m_paucRecvBuffer;
    delete[] m_pacSendBuffer;
    delete[] m_paucSendBuffer;

    qDebug() << "CommDisplay";
}


bool CommDisplay::InitPort(QString qstrPort)
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

void CommDisplay::on_SignalInitPort()
{
    m_serialPort = new QSerialPort;
    InitPort(m_pGParam->m_SCommNetSettings.qstrPortDisplay);
    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(receiveInfo()));
}

void CommDisplay::receiveInfo()
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
            emit Signal25Hz();
        }
    }
}

void CommDisplay::ProcessRecvData(void)
{
    m_iRecvFramePerSec++;	// 接收帧数自加
    unsigned char* paucRecvBuffer = (unsigned char*)m_pacRecvBuffer;	// char数组转换为unsigned char数组
//	m_iRecvTimeMarkNoChange = paucRecvBuffer[17] == m_ucRecvTimeMark ? m_iRecvTimeMarkNoChange + 1 : 0;	// 判断时标是否未变
//	if (m_iRecvTimeMarkNoChange < 10)	// 判断时标连续未变是否超限
    {
        m_pSGlobalData->iCommErrorRecvTimeMark = STATUS_OK;
        /// 解码
        m_pSGlobalData->sexpdispdataRecv.iDayRecv = 0;
        m_pSGlobalData->sexpdispdataRecv.iYMDYear = 2000 + (paucRecvBuffer[1] >> 4) * 10 + (paucRecvBuffer[1] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iYMDMonth = ((paucRecvBuffer[2] & 0x10) >> 4) * 10
                + (paucRecvBuffer[2] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iYMDDay = ((paucRecvBuffer[3] & 0x20) >> 5) * 20
                + ((paucRecvBuffer[3] & 0x10) >> 4) * 10
                + (paucRecvBuffer[3] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iHourRecv = ((paucRecvBuffer[4] & 0x20) >> 5) * 20
            + ((paucRecvBuffer[4] & 0x10) >> 4) * 10
            + (paucRecvBuffer[4] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iMinuteRecv = ((paucRecvBuffer[5] & 0x40) >> 6) * 40
            + ((paucRecvBuffer[5] & 0x20) >> 5) * 20
            + ((paucRecvBuffer[5] & 0x10) >> 4) * 10
            + (paucRecvBuffer[5] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iSecondRecv = ((paucRecvBuffer[6] & 0x40) >> 6) * 40
            + ((paucRecvBuffer[6] & 0x20) >> 5) * 20
            + ((paucRecvBuffer[6] & 0x10) >> 4) * 10
            + (paucRecvBuffer[6] & 0x0F);
        m_pSGlobalData->sexpdispdataRecv.iMilliSecondRecv = ((paucRecvBuffer[8] << 8) + paucRecvBuffer[7]) / 10;
        m_pSGlobalData->sexpdispdataRecv.iMicroSecondRecv = 0;
        unsigned long ulAzimuthCode = (paucRecvBuffer[12] << 24) + (paucRecvBuffer[11] << 16) + (paucRecvBuffer[10] << 8) + paucRecvBuffer[9];
        m_pSGlobalData->sexpdispdataRecv.dAzimuthDegreeTotalRecv = double(ulAzimuthCode) / pow(2.0, 29.0) * 360.0;
        m_pSGlobalData->sexpdispdataRecv.iAzimuthDegreeRecv = int(double(ulAzimuthCode) / pow(2.0, 29.0) * 360.0);
        m_pSGlobalData->sexpdispdataRecv.iAzimuthMinuteRecv = int((double(ulAzimuthCode) / pow(2.0, 29.0) * 360.0 - double(m_pSGlobalData->sexpdispdataRecv.iAzimuthDegreeRecv)) * 60.0);
        m_pSGlobalData->sexpdispdataRecv.iAzimuthSecondRecv = int((double(ulAzimuthCode) / pow(2.0, 29.0) * 360.0 - double(m_pSGlobalData->sexpdispdataRecv.iAzimuthDegreeRecv)) * 3600.0) - m_pSGlobalData->sexpdispdataRecv.iAzimuthMinuteRecv * 60;
        unsigned long ulElevationCode = (paucRecvBuffer[16] << 24) + (paucRecvBuffer[15] << 16) + (paucRecvBuffer[14] << 8) + paucRecvBuffer[13];
        m_pSGlobalData->sexpdispdataRecv.dElevationDegreeTotalRecv = double(ulElevationCode) / pow(2.0, 29.0) * 360.0;
        m_pSGlobalData->sexpdispdataRecv.iElevationDegreeRecv = int(double(ulElevationCode) / pow(2.0, 29.0) * 360.0);
        m_pSGlobalData->sexpdispdataRecv.iElevationMinuteRecv = int((double(ulElevationCode) / pow(2.0, 29.0) * 360.0 - double(m_pSGlobalData->sexpdispdataRecv.iElevationDegreeRecv)) * 60.0);
        m_pSGlobalData->sexpdispdataRecv.iElevationSecondRecv = int((double(ulElevationCode) / pow(2.0, 29.0) * 360.0 - double(m_pSGlobalData->sexpdispdataRecv.iElevationDegreeRecv)) * 3600.0) - m_pSGlobalData->sexpdispdataRecv.iElevationMinuteRecv * 60;
    }
    //else // 时标连续未变超限
    //{
    //	m_pSGlobalData->iCommErrorRecvTimeMark = STATUS_ERROR;
    //}
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

void CommDisplay::ProcessSendData(int iProcMode)
{
    if (iProcMode == 0)
    {
        m_iSendFramePerSec++;	// 发送帧数自加
        memset(m_pacSendBuffer, 0, m_iSendBytes);
        {
            QMutexLocker locker(&m_pSGlobalData->qmutexSend);
            /// 编码

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
    }
}

void CommDisplay::on_SignalTimerRecvCheck(void)
{
    m_pSGlobalData->iCommErrorRecvLost = m_iRecvFramePerSec > 0 ? STATUS_OK : STATUS_ERROR;
    m_iRecvFramePerSec = 0;
    if (m_pSGlobalData->iCommErrorRecvLost == STATUS_ERROR)
    {
        if (m_serialPort->isOpen())
        {
            m_serialPort->clear();
            m_serialPort->close();
        }
        m_bPortIsOpen = m_serialPort->open(QIODevice::ReadWrite);
        m_pLog->CriticalMsg(QStringLiteral("25Hz数据端口接收帧数丢失超限."));
    }
}
