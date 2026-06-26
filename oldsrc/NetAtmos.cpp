#include "NetAtmos.h"

NetAtmos::NetAtmos()
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SNetErrorDiagnoseData.pvoidThis = (void*)this;
    m_pqsktComm = new QUdpSocket;
    m_qstrIPLocal = m_pGParam->m_SCommNetSettings.qstrIPLocal;
    m_qstrIPAtmos = "";
    m_iPortAtmos = m_pGParam->m_SCommNetSettings.iPortAtmos;
    m_pGParam->m_sNetAtmosData.iStatusConnect = m_pqsktComm->bind(QHostAddress::AnyIPv4, m_iPortAtmos) ? STATUS_OK : STATUS_ERROR;
    connect(m_pqsktComm, SIGNAL(readyRead()), this, SLOT(receive()));

    m_pqthdComm = new QThread;
//    moveToThread(m_pqthdComm);
    m_pqthdComm->start();
}

#include <QDebug>
NetAtmos::~NetAtmos()
{
    //delete m_pqthdComm;
    delete m_pqsktComm;

    qDebug() << "NetAtmos";
}

void NetAtmos::receive()
{
    m_pqsktComm->readDatagram((char*)&m_RecvPacket, sizeof(m_RecvPacket), 0, 0);
    m_pGParam->m_sNetAtmosData.fTemp = m_RecvPacket.temperature;
    m_pGParam->m_sNetAtmosData.fHumidity = m_RecvPacket.humidity;
    m_pGParam->m_sNetAtmosData.fAtmosP = m_RecvPacket.pressure * 100.0;
}
