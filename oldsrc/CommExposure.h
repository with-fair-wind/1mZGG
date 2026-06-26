#ifndef COMMEXPOSURE_H
#define COMMEXPOSURE_H

#include <math.h>
#include "GlobalParameter.h"
#include <QSerialPort>
#include <QThread>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QDir>
#include <QtCore/QWaitCondition>
#include "MyLog.h"
#include <QDebug>
#include <QTimer>
#include "Grabber.h"

class CommExposure : public QObject
{
Q_OBJECT

/// 函数
public:
    CommExposure(void);
    ~CommExposure(void);

private:
    bool InitPort(QString qstrPort);	// 初始化端口
    void ProcessRecvData(void);	// 处理接收数据
    void ProcessSendData(int iProcMode);	// 处理发送数据

signals:
    void SignalInitPort(void);
    void SignalExpComm(void);

private slots:
    void on_SignalInitPort(void);
    void receiveInfo(void);
    void sendInfo(void);
    void on_SignalTimerRecvCheck(void);

/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    SCommExposureData* m_pSGlobalData;	// 对应的全局数据结构体指针(定义于GlobalParameter类)
    QThread* m_qthdRun;
    QSerialPort* m_serialPort;
    QString m_qstrPort;	// 端口号
    bool m_bPortIsOpen;	// 端口状态(打开/关闭)
    int m_iRecvFrameFrequency;	// 接收帧频
    int m_iRecvFramePerSec;	// 每秒实际接收帧数
    unsigned char m_ucRecvTimeMark;	// 接收时标
    int m_iRecvTimeMarkNoChange;	// 接收时标连续停滞数量
    int m_iRecvBytes;	// 接收帧字节数
    char m_cRecvFrameBegin;	// 接收帧头
    char m_cRecvFrameEnd;	// 接收帧尾
    QVector<char> m_qvectorcharRecvFIFO;	// 接收FIFO
    char* m_pacRecvBuffer;	// 接收缓存指针(用于接收)
    unsigned char* m_paucRecvBuffer;	// 接收缓存指针(用于类外调用)
    int m_iSendFrameFrequency;	// 发送帧频
    int m_iSendFramePerSec;	// 每秒实际发送帧数
    unsigned char m_ucSendTimeMark;	// 发送时标
    int m_iSendBytes;	// 发送帧字节数
    bool m_bSendStatus;	// 发送状态(sio_putch返回值==1的连续逻辑与)
    char* m_pacSendBuffer;	// 发送缓存指针
    unsigned char* m_paucSendBuffer;	// 发送缓存指针(用于类外调用)
    MyLog* m_pLog;
    QTimer* m_pqTimer;
    Grabber* m_pGrabber;
};

#endif // COMMEXPOSURE_H
