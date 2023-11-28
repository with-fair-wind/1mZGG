#include "NetImageSender.h"

NetImageSender::NetImageSender()
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pSendThread = new StandardThread(CallBackSend, (void*)this);
    m_pqsktComm = new QUdpSocket(this);
    m_qstrIPLocal = m_pGParam->m_SCommNetSettings.qstrIPLocal;
    m_qstrIPImageTrans = m_pGParam->m_SCommNetSettings.qstrIPImageTrans;
    m_iPortImageTrans = m_pGParam->m_SCommNetSettings.iPortImageTrans;
    m_pGParam->m_SNetImageSenderData.iStatusConnect = m_pqsktComm->bind(QHostAddress(m_qstrIPLocal), m_iPortImageTrans) ? STATUS_OK : STATUS_ERROR;
    m_paucImage = NULL;
    m_paucImage = NULL;
    m_pacEncode = NULL;
    m_uiImageWidth = 0;
    m_uiImageHeight = 0;
    m_pImageProc = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
    m_pSendThread->start(QThread::HighPriority);
}
#include <QDebug>
NetImageSender::~NetImageSender()
{
    m_qmEventSend.lock();
    m_pSendThread->m_stopRequested = true;
    m_qwcEventSend.wakeAll();
    m_qmEventSend.unlock();
    delete m_pSendThread;
    if(m_paucImage) delete[] m_paucImage;
    if(m_pacEncode) delete[] m_pacEncode;

    qDebug() << "NetImageSender";
}

QWaitCondition NetImageSender::m_qwcEventSend;
QMutex NetImageSender::m_qmEventSend;

void NetImageSender::CallBackSend(void* pvoidThis)
{
    NetImageSender* pThis = (NetImageSender*)pvoidThis;
    QMutexLocker locker(&m_qmEventSend);
    while (!pThis->m_pSendThread->m_stopRequested)
    {
        m_qwcEventSend.wait(&m_qmEventSend);
        if (!pThis->m_pSendThread->m_stopRequested)
        {
            /// 数据编码
            unsigned long ulPacketLen = 0;
            pThis->EncodeImageData(pThis->m_pacEncode, pThis->m_paucImage, &ulPacketLen);
            /// 数据拆包与发送
            pThis->PackageSendUDP(pThis->m_pacEncode, ulPacketLen);
        }
    }
}

void NetImageSender::EncodeImageData(char* pacDst, unsigned char* paucSrc, unsigned long* pulPacketLen)
{
    char acHead[10];
    WriteHeader(acHead);
    memcpy(pacDst, acHead, 10 * sizeof(char));
    memcpy(pacDst + 10, paucSrc, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned char));
    *pulPacketLen = m_uiImageWidth * m_uiImageHeight + 10;
}

void NetImageSender::WriteHeader(char* pacBuffer)
{
    unsigned long ulPacketLen = m_uiImageWidth * m_uiImageHeight + 10;
    *pacBuffer++ = (char)((ulPacketLen >> 24) & 0xff);
    *pacBuffer++ = (char)((ulPacketLen >> 16) & 0xff);
    *pacBuffer++ = (char)((ulPacketLen >> 8) & 0xff);
    *pacBuffer++ = (char)(ulPacketLen & 0xff);
    *pacBuffer++ = (char)((m_uiImageWidth >> 8) & 0xff);
    *pacBuffer++ = (char)(m_uiImageWidth & 0xff);
    *pacBuffer++ = (char)((m_uiImageHeight >> 8) & 0xff);
    *pacBuffer++ = (char)(m_uiImageHeight & 0xff);
    *pacBuffer++ = 1;   // 通道数 1:Mono 3:RGB
    *pacBuffer++ = 8;  // 灰度位深 8:8bit 16:16bit
}

void NetImageSender::PackageSendUDP(char* pacBuffer, unsigned long ulPacketLen)
{
    unsigned int uiSliceLen = 1024 * 60;
    unsigned int uiNumSlice = ulPacketLen % uiSliceLen == 0 ? (ulPacketLen / uiSliceLen) : (ulPacketLen / uiSliceLen + 1);
    unsigned int uiMiniPacketLen = 5 * 4 + uiSliceLen + 4;  // 20个字节的包头+1024个字节的数据域+4个字节的空包尾(除最后一包)
    char* pacMiniPacket = new char[uiMiniPacketLen];
    for (int i = 0; i < uiNumSlice; i++)
    {
        memset(pacMiniPacket, 0, uiMiniPacketLen * sizeof(char));
        uint32_t ui32Head = 0xAAAA5555;
        uint32_t ui32NumPacket = uiNumSlice;
        uint32_t ui32PacketLen = ulPacketLen;
        uint32_t ui32SeqCurrent = i + 1;
        uint32_t ui32CurrentLen = uiSliceLen;
        if (i == uiNumSlice - 1)
        {
            ui32CurrentLen = ulPacketLen % uiSliceLen;
        }
        memcpy(pacMiniPacket, &ui32Head, sizeof(uint32_t));
        memcpy(pacMiniPacket + 4, &ui32NumPacket, sizeof(uint32_t));
        memcpy(pacMiniPacket + 8, &ui32PacketLen, sizeof(uint32_t));
        memcpy(pacMiniPacket + 12, &ui32SeqCurrent, sizeof(uint32_t));
        memcpy(pacMiniPacket + 16, &ui32CurrentLen, sizeof(uint32_t));
        memcpy(pacMiniPacket + 20, pacBuffer + i * uiSliceLen, ui32CurrentLen * sizeof(char));
        m_pqsktComm->writeDatagram(pacMiniPacket, 20 + ui32CurrentLen + 4, QHostAddress(m_qstrIPImageTrans), m_iPortImageTrans);
        QThread::msleep(2);
    }
    delete []  pacMiniPacket;
}

void NetImageSender::on_SignalSend()
{
    unsigned int uiWidth, uiHeight;
    unsigned char* paucDisp = m_pImageProc->GetDispImage(uiWidth, uiHeight);
    if (paucDisp)
    {
        if (m_uiImageWidth != uiWidth || m_uiImageHeight != uiHeight)
        {
            if (m_paucImage) delete [] m_paucImage;
            m_uiImageWidth = uiWidth;
            m_uiImageHeight = uiHeight;
            m_paucImage = new unsigned char[m_uiImageWidth * m_uiImageHeight];
            memset(m_paucImage, 0, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned char));
            if (m_pacEncode) delete [] m_pacEncode;
            m_pacEncode = new char[m_uiImageWidth * m_uiImageHeight + 10];
            memset(m_pacEncode, 0, (m_uiImageWidth * m_uiImageHeight + 10) * sizeof(char));
        }
        memcpy(m_paucImage, paucDisp, m_uiImageWidth * m_uiImageHeight * sizeof(unsigned char));
        m_qwcEventSend.wakeAll();
    }
}
