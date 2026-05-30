#ifndef ENETSERVER_H
#define ENETSERVER_H


//socket
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

//#include <winsock2.h>
#include "enet/enet.h"
#include <QThread>

//#pragma comment(lib,"ws2_32.lib")
//#pragma comment(lib, "winmm")

class ENetServer : public QThread
{
    Q_OBJECT

/// 函数
public:
    ENetServer(unsigned int port);
    virtual ~ENetServer();
    void Send(const char* data0, int len, enet_uint32 flag = ENET_PACKET_FLAG_RELIABLE);
    bool IsReady();
    char* GetPacketData(unsigned long& ulPacketLength);
    bool GetConnect() { return connected; }

protected:
    void run();

private:
    void ProcessData(char* data, int len);

signals:
    void SignalRecvData();

/// 变量
private:
    ENetAddress address;
    ENetHost *server;
    ENetEvent event;
    bool connected;
    char* data;
    size_t dataLen;
    size_t packetLen;
    size_t m_connCounter;   ///客户端连接数
    int channel;
    bool bInit;
};

#endif // ENETSERVER_H
