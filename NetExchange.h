#ifndef NETEXCHANGE_H
#define NETEXCHANGE_H

#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QTimer>
#include "GlobalParameter.h"

#pragma pack(push)
#pragma pack(push,1)

struct sGXTCHeader
{
    int iNumTargets;
    long lJMS1970;
    char cMeasureStatus;
    char cTaskMode;
    char cTargetStatus;
    double dTemp;
    double dAtmosP;
    double dHumidity;
};

struct sGXTCData
{
    char cMainFlag;
    int iTargetID;
    int iAzi;
    int iEle;
    int iAziSpd;
    int iEleSpd;
    double dMv = 0;
};

struct sGDCLData
{
    long lJMS1970;
    char cMeasureStatus;
    int iTargetID;
    char cDataFlag;
    int iDN;
    int iMv;
    long lDist;
    int iAlpha;
    int iDelta;
    int iMvRes;
};

#pragma pack(pop)

class NetExchange : public QObject
{
    Q_OBJECT

    /// 函数
public:
    NetExchange(void);
    ~NetExchange(void);
    void SendGXTC(char* pacData, int iNumBytes);
    void SendGDCL(char* pacData, int iNumBytes);

signals:
    /// 变量
private:
    GlobalParameter* m_pGParam;
    QThread* m_pqthdComm;
    QUdpSocket* m_pqsktCommGXTC;
    QUdpSocket* m_pqsktCommGDCL;
    QString m_qstrIPLocal;
    QString m_qstrIPExchange;
    int m_iPortExchangeGXTC;
    int m_iPortExchangeGDCL;
};

#endif // NETEXCHANGE_H



