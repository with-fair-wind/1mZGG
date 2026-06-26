#ifndef COMMMASTERCONTROL
#define COMMMASTERCONTROL

#include <math.h>
#include "GlobalParameter.h"
#include <QSerialPort>
#include <QThread>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QDir>
#include <QDebug>
#include <utility>
#include <QWaitCondition>
#include <QMutex>
#include "MyLog.h"
#include "ImageProcessor.h"

class CommMasterControl : public QObject
{
Q_OBJECT

/// 函数
public:
    CommMasterControl(void);
    ~CommMasterControl(void);

private:
    bool InitPort(QString qstrPort);	// 初始化端口
    void ProcessRecvData(void);	// 处理接收数据
    void ProcessSendData(int iProcMode);	// 处理发送数据
    void ProcessRecvCheck(void);	// 处理接收检查
    void DateMinus1(unsigned int uiYear, unsigned int uiMonth, unsigned int uiDay, unsigned int& uiYearNew, unsigned int& uiMonthNew, unsigned int& uiDayNew);

signals:
    void SignalInitPort(void);
//    void SignalMCSetExp(float fExpTime);
//    void SignalMCSetTrackMode(int iIndexComboTrackMode);
//    void SignalMCSetTrack(bool bTrack);
//    void SignalMCSetImageSave(bool bImageSave);
//    void SignalMCSetTaskChanged(void);
    void SignalMCSet(float fExpTime, int iIndexComboTrackMode, bool bTrack, bool bImageSave);

private slots:
    void on_SignalInitPort(void);
    void receiveInfo(void);
    void sendInfo(void);

/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    SCommMasterControlData* m_pSGlobalData;	// 对应的全局数据结构体指针(定义于GlobalParameter类)
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
    float m_fExpTime;
    unsigned int m_uiMode1;
    unsigned int m_uiMode2;
    bool m_bTrack;
    bool m_bImageSave;
    unsigned int m_uiTargetID;
    unsigned int m_uiTaskID;
    unsigned int m_uiHourStart;
    unsigned int m_uiMinuteStart;
    unsigned int m_uiSecondStart;
    unsigned int m_uiHourEnd;
    unsigned int m_uiMinuteEnd;
    unsigned int m_uiSecondEnd;
};

#endif // COMMMASTERCONTROL_H
