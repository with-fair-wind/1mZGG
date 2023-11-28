#include "NetExchange.h"

NetExchange::NetExchange()
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SNetExchangeData.pvoidThis = (void*)this;
    m_pqsktCommGXTC = new QUdpSocket;
    m_pqsktCommGDCL = new QUdpSocket;
    m_qstrIPLocal = m_pGParam->m_SCommNetSettings.qstrIPLocal;
    m_qstrIPExchange = m_pGParam->m_SCommNetSettings.qstrIPExchange;
    m_iPortExchangeGXTC = m_pGParam->m_SCommNetSettings.iPortExchangeGXTC;
    m_iPortExchangeGDCL = m_pGParam->m_SCommNetSettings.iPortExchangeGDCL;
    m_pGParam->m_SNetExchangeData.iStatusConnect = m_pqsktCommGXTC->bind(QHostAddress(m_qstrIPLocal), m_iPortExchangeGXTC) ? STATUS_OK : STATUS_ERROR;
    m_pGParam->m_SNetExchangeData.iStatusConnect = m_pqsktCommGDCL->bind(QHostAddress(m_qstrIPLocal), m_iPortExchangeGDCL) ? m_pGParam->m_SNetExchangeData.iStatusConnect : STATUS_ERROR;

    m_pqthdComm = new QThread;
//    moveToThread(m_pqthdComm);
    m_pqthdComm->start();
}

#include <QDebug>
NetExchange::~NetExchange()
{
    //delete m_pqthdComm;
    delete m_pqsktCommGXTC;
    delete m_pqsktCommGDCL;

    qDebug() << "NetExchange";
}

void NetExchange::SendGXTC(char *pacData, int iNumBytes)
{
    if (m_pqsktCommGXTC != NULL)
        m_pqsktCommGXTC->writeDatagram(pacData, iNumBytes, QHostAddress(m_qstrIPExchange), m_iPortExchangeGXTC);
}

void NetExchange::SendGDCL(char *pacData, int iNumBytes)
{
    if (m_pqsktCommGDCL != NULL)
        m_pqsktCommGDCL->writeDatagram(pacData, iNumBytes, QHostAddress(m_qstrIPExchange), m_iPortExchangeGDCL);
}

