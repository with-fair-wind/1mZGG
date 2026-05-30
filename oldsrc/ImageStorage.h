#ifndef IMAGESTORAGE_H
#define IMAGESTORAGE_H

#include <QTime>
#include <QtCore/QWaitCondition>
#include <QDataStream>
#include <QDebug>
#include <fstream>
#include <iostream>
#include "StandardThread.h"
#include "GlobalParameter.h"
#include <vector>
#include "ImageCode.h"


struct sStoragePacket
{
    QString qstrFileName = "";
    SExposureDisplayData sexpdispdataCurrent;
    unsigned short* pausImage = NULL;
    int iTriggerModeCurrent = 0;
    double dFrameFrequencyCurrent = 0;
    double dExposureTimeCurrent = 0;
};


class ImageStorage : public QObject
{
Q_OBJECT

/// 函数
public:
    ImageStorage(void);
    ~ImageStorage(void);
    void SetMatchTimeStore(bool bMatchTimeStore);
    void SetStore(bool bStore);
    bool GetStore(void) { return m_bStore; }

private:
    static void CallBackStore(void* pvoidThis);	// 存储线程回调
    void GenerateHeader(sStoragePacket*);	// 生成文件头
    void WriteHeader(char* pacBuffer);	// 写文件头
    void StoreImageData(sStoragePacket*);	// 存储图像数据
    void WriteIFM(void);

private slots :
    void on_SignalStartGrab(unsigned int uiImageWidth, unsigned int uiImageHeight);	// 开始采集信号响应槽
    void on_SignalGrabDataRotate(void);	// 采集到图像信号响应槽

/// 变量
public:
    static QWaitCondition m_qwcEventStore;
    static QMutex m_qmEventStore;	// 存储事件

private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    StandardThread* m_pStoreThread;	// 存储线程对象指针
    bool m_bStartGrab;	// 开始采集标识
    QString m_qstrRootPath;	// 图像保存根目录
    QString m_qstrStorePath;	// 图像保存路径(每次开始采集后自动生成)
    bool m_bMatchTimeStore;
    bool m_bStore;
    unsigned int m_uiImageWidth;
    unsigned int m_uiImageHeight;
    SExposureDisplayData m_sexpdispdataCurrent;
    double m_dExposureTimeCurrent;
    int m_iTriggerModeCurrent;
    double m_dFrameFrequencyCurrent;
    SImageFileHeader m_simagefileheaderWrite;	// 待写入图像文件头
    unsigned long long m_ullStorageSeq;
    std::vector<sStoragePacket*> m_vectFIFO;
};

#endif // IMAGESTORAGE_H
