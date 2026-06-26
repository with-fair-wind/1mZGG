#ifndef NETAPP_H
#define NETAPP_H


#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QTimer>

class NetApp : public QObject
{
Q_OBJECT

    /// 函数
public:
    NetApp(void);
    ~NetApp(void);

signals:
    void SignalRecv(void);
    void SignalInit(void);

private slots:
    void receive();
    void on_SignalTimeOut();
    void on_SignalCloseCuard();
    void on_SignalInit(void);

    /// 变量
private:
    QThread* m_pqthdComm;
    QUdpSocket* m_pqsktComm;
    QString m_qstrIPLocal;
    int m_iPort;
    QTimer* m_pTimer;
    bool m_bClose;
};

#endif // NETAPP_H
