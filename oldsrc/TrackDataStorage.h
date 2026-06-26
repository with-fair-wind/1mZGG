#ifndef TRACKDATASTORAGE_H
#define TRACKDATASTORAGE_H


#include <QTime>
#include "StandardThread.h"
#include "GlobalParameter.h"
#include "ImageProcessor.h"
#include "Grabber.h"

class TrackDataStorage : public QObject
{
Q_OBJECT

/// 函数
public:
    TrackDataStorage(void);
    ~TrackDataStorage(void);
    void SetFileName(QString qstrFileName);
    QString GetFileName(void);
    void SetStore(bool bStore);

private:
    static void CallBackStore(void* pvoidThis);	// 存储线程回调
    void StoreData(void);	// 存储数据

private slots :
    void on_SignalTrackData(void);	// 提取到跟踪数据信号响应槽

/// 变量
public:
    static QMutex m_qmEventStore;	// 存储事件
    static QWaitCondition m_qwcEventStore;
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    StandardThread* m_pStoreThread;	// 存储线程对象指针
    bool m_bStore;	// 开始存储标识
    QString m_qstrStorePath;	// 保存路径(每次开始采集后自动生成)
    ImageProcessor* m_pImageProcessor;
    Grabber* m_pGrabber;
};

#endif // TRACKDATASTORAGE_H
