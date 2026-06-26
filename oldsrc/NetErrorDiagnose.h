#ifndef NETERRORDIAGNOSE_H
#define NETERRORDIAGNOSE_H


#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include "GlobalParameter.h"

class NetErrorDiagnose : public QObject
{
Q_OBJECT

    /// 函数
public:
    NetErrorDiagnose(void);
    ~NetErrorDiagnose(void);

signals:
    void SignalRecv(void);

private slots:
    void receive();
    void on_SignalSendTimer(void);

    /// 变量
private:
    GlobalParameter* m_pGParam;
    QThread* m_pqthdComm;
    QUdpSocket* m_pqsktComm;
    QString m_qstrIPLocal;
    QString m_qstrIPErrDiag;
    int m_iPortErrDiag;
    QTimer* m_qtimerSend;
};

#endif // NETERRORDIAGNOSE_H
