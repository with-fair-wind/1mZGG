#include "NetErrorDiagnose.h"

NetErrorDiagnose::NetErrorDiagnose()
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SNetErrorDiagnoseData.pvoidThis = (void*)this;
    m_pqsktComm = new QUdpSocket;
    m_qstrIPLocal = m_pGParam->m_SCommNetSettings.qstrIPLocal;
    m_qstrIPErrDiag = m_pGParam->m_SCommNetSettings.qstrIPErrDiag;
    m_iPortErrDiag = m_pGParam->m_SCommNetSettings.iPortErrDiag;
    m_pGParam->m_SNetErrorDiagnoseData.iStatusConnect = m_pqsktComm->bind(QHostAddress(m_qstrIPLocal), m_iPortErrDiag) ? STATUS_OK : STATUS_ERROR;
    connect(m_pqsktComm, SIGNAL(readyRead()), this, SLOT(receive()));
    m_qtimerSend = new QTimer(this);
    connect(m_qtimerSend, SIGNAL(timeout()), this, SLOT(on_SignalSendTimer()));

    m_pqthdComm = new QThread;
//    moveToThread(m_pqthdComm);
    m_pqthdComm->start();

    m_qtimerSend->start(1000);
}
#include <QDebug>
NetErrorDiagnose::~NetErrorDiagnose()
{
    //delete m_pqthdComm;
    delete m_pqsktComm;

    qDebug() << "NetErrorDiagnose";
}


void NetErrorDiagnose::receive()
{
}

void NetErrorDiagnose::on_SignalSendTimer(void)
{
    QJsonObject obj;

    bool bCommStatus = (m_pGParam->m_SCommDisplayData.iCommErrorRecvLost != STATUS_ERROR)
            && (m_pGParam->m_SCommExposureData.iCommErrorRecvLost != STATUS_ERROR);
    obj.insert("DPS001", bCommStatus ? 1 : 0);

    bool bCameraStatus = m_pGParam->m_SGrabberData.iRecvStatus != STATUS_ERROR;
    obj.insert("DPS002", bCameraStatus ? 1 : 0);

    bool bStorageStatus = m_pGParam->m_SImageStorageData.iStorageStatus != STATUS_ERROR;
    obj.insert("DPS003", bStorageStatus ? 1 : 0);

    obj.insert("DPS004", 1);

    QJsonDocument doc = QJsonDocument(obj);
    QByteArray sendArr = doc.toJson();

    m_pqsktComm->writeDatagram(sendArr, sendArr.size(), QHostAddress(m_qstrIPErrDiag), m_iPortErrDiag);
}
