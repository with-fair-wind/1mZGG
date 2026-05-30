#include "NetApp.h"

NetApp::NetApp()
{
    m_bClose = false;

    m_pqthdComm = new QThread;
    moveToThread(m_pqthdComm);
    m_pqthdComm->start();

    connect(this, SIGNAL(SignalInit()), this, SLOT(on_SignalInit()));
    emit SignalInit();
}

NetApp::~NetApp()
{
    m_pqsktComm->deleteLater();

    m_pqthdComm->quit();
    m_pqthdComm->wait();
}

void NetApp::on_SignalInit()
{
    m_pqsktComm = new QUdpSocket;
    m_pqsktComm->bind(QHostAddress::AnyIPv4, 15361);
    connect(m_pqsktComm, SIGNAL(readyRead()), this, SLOT(receive()));

    m_pTimer = new QTimer;
    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(on_SignalTimeOut()));
    m_pTimer->start(100);
}

void NetApp::receive()
{
    char pacRecv[10];
    memset(pacRecv, 0, 10);
    m_pqsktComm->readDatagram(pacRecv, 10, 0, 0);
}

void NetApp::on_SignalTimeOut()
{
    char pacData[10];
    memset(pacData, 0, 10);
    pacData[0] = 0x7E;
    pacData[1] = m_bClose ? 0x01 : 0x00;
    pacData[9] = 0xE7;
    m_pqsktComm->writeDatagram(pacData, 10, QHostAddress::AnyIPv4, 15362);
}

void NetApp::on_SignalCloseCuard()
{
    m_bClose = true;
}


