#ifndef NETIMAGESENDER_H
#define NETIMAGESENDER_H

#include <stdint.h>
#include <QObject>
#include <QUdpSocket>
#include <QWaitCondition>
#include <QMutex>
#include "GlobalParameter.h"
#include "StandardThread.h"
#include "ImageProcessor.h"

class NetImageSender : public QObject
{
Q_OBJECT

/// 函数
public:
    NetImageSender(void);
    ~NetImageSender(void);

private:
    static void CallBackSend(void* pvoidThis);
    void EncodeImageData(char* pacDst, unsigned char* paucSrc, unsigned long* pulPacketLen);
    void WriteHeader(char* pacBuffer);
    void PackageSendUDP(char* pacBuffer, unsigned long ulPacketLen);

private slots:
    void on_SignalSend(void);

/// 变量
public:
    static QWaitCondition m_qwcEventSend;
    static QMutex m_qmEventSend;

private:
    GlobalParameter* m_pGParam;
    StandardThread* m_pSendThread;
    QUdpSocket* m_pqsktComm;
    QString m_qstrIPLocal;
    QString m_qstrIPImageTrans;
    int m_iPortImageTrans;
    unsigned char* m_paucImage;
    char* m_pacEncode;
    unsigned int m_uiImageWidth;
    unsigned int m_uiImageHeight;
    ImageProcessor* m_pImageProc;
};

#endif // NETIMAGESENDER_H





















































