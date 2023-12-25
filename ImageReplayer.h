#ifndef IMAGEREPLAYER_H
#define IMAGEREPLAYER_H


#include <QTime>
#include <QTimer>
#include <QVector>
#include <QDir>
#include <QMessageBox>
#include <QtCore/QWaitCondition>
#include "StandardThread.h"
#include "GlobalParameter.h"
#include "ImageCode.h"

class ImageReplayer : public QObject
{
Q_OBJECT

/// 函数
public:
    ImageReplayer(void);
    ~ImageReplayer(void);
    bool SetReplayPath(QString qstrReplayPath);
    bool SetReplay(bool bReplayDirection);
    bool SetBrowse(bool bBrowseDirection);
    bool SetPause(void);
    QString GetReplayPath(void);
    bool GetImageValid(void) { return m_bImageValid; }
    unsigned GetCurImgID(){return m_uiSeqCurrent;}

private:
    static void CallBackReplay(void* pvoidThis);	// 回放线程回调
    bool GetHeader(void);
    void GetImage(void);	// 获取图像
    bool GetHeaderBMP(void);
    void GetBMP(bool bFirstGetBMP);
    void SetReplayInterval(char* pacFrameFrequency);	// 设置播放帧间隔
    void SetReplayInterval(double dFrameFrequency);
    void CreatDir(QString qstrPath);

//    void GenerateHeader(void);
//    void WriteHeader(char* pacBuffer);
//    SImageFileHeader m_simagefileheaderWrite;	// 待写入图像文件头

private slots:
    void on_qtimerReplay_timeout(void);	// 回放定时器溢出响应槽

signals:
    void SignalReplayData(void);	// 回放信号(ImageProcessor接收)
    void SignalReplayInit(unsigned int uiGrabWidth, unsigned int uiGrabHeight);
    void SignalChangeUITrackMode(int iMode);  // iMode==0：切换为‘无处理（全帧）’;iMode==1：切换为‘无处理（开窗）’;iMode==2：图像宽高不匹配
    void SignalAddEnding();
    void SignalAddProc(int iSeqCut, int iNumTotal);

public slots:
    void on_SignalAddImage();

/// 变量
private:
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    StandardThread* m_pReplayThread;	// 回放线程对象指针
    static QWaitCondition m_qwcEventReplay;// 回放事件
    static QMutex m_qmEventReplay;
    QTimer* m_pqtimerReplay;	// 回放定时器对象指针
    bool m_bReplay;	// 回放标识
    bool m_bBrowse;	// 浏览标识
    bool m_bDirection;	// 回放或浏览方向 true:正向 false:反向
    int m_iReplayInterval;	// 回放时间间隔 ms
    QStringList m_qstrlistReplayFileName;	// 回放文件名序列
    unsigned int m_uiSeqCurrent;	// 当前序号
    QString m_qstrReplayPath;
    bool m_bImageValid;	// 图像有效标识
    unsigned int m_uiImageWidth;
    unsigned int m_uiImageHeight;
};

#endif // IMAGEREPLAYER_H
