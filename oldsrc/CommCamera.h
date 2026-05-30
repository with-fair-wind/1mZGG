#ifndef COMMCAMERA_H
#define COMMCAMERA_H

#include "GlobalParameter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <QThread>

class CommCamera
{
/// 函数
public:
    CommCamera(void);
    ~CommCamera(void);
    bool GetPortStatus(void) { return m_bPortIsOpen; }
    bool SendData(QByteArray message);
    bool SerialInit(bool bInited);
    void StartGrab(bool bStart);
    char* TranslateData(char RB, char ADD, char Data);
    int OnSetTimeOfExposure(QString buffer);
    int OnSetGain(QString buffer);
    void OnSetWindow(int line,int start);
    void OnQueryTemp(float& fTemp);
    void OnTrainCam(void);

signals:

/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    SCommCameraData* m_pSGlobalData;	// 对应的全局数据结构体指针(定义于GlobalParameter类)
    bool m_bPortIsOpen;	// 端口状态(打开/关闭)
    int m_iSerialPort; //串口文件
    char m_ccommand[3];
    struct termios stNew;
    bool m_bQueryTemp;
};

#endif // CAMERACOMM_H
