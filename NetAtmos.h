#ifndef NETATMOS_H
#define NETATMOS_H

#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QTimer>
#include "GlobalParameter.h"

#pragma pack(push)
#pragma pack(1)

struct Receive_From_Weather
{
    int frame_head;
    double temperature;
    double humidity;
    double pressure;
};

#pragma pack(pop)

class NetAtmos : public QObject
{
Q_OBJECT

    /// 函数
public:
    NetAtmos(void);
    ~NetAtmos(void);

private slots:
    void receive();

    /// 变量
private:
    GlobalParameter* m_pGParam;
    QThread* m_pqthdComm;
    QUdpSocket* m_pqsktComm;
    QString m_qstrIPLocal;
    QString m_qstrIPAtmos;
    int m_iPortAtmos;
    Receive_From_Weather m_RecvPacket;
};

#endif // NETATMOS_H
