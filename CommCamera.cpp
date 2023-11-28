#include "CommCamera.h"

CommCamera::CommCamera(void)
{
    /// 成员变量
    m_pGParam = GlobalParameter::GetInstance();
    m_pGParam->m_SGrabberData.pvoidComm=(void*)this;
    m_iSerialPort=0;
    m_bPortIsOpen = false;
    m_bQueryTemp = false;
}

#include <QDebug>
CommCamera::~CommCamera(void)
{
    QByteArray m_badata="\x51\x07\x03";
    int nByte = write(m_iSerialPort, m_badata.data() ,m_badata.length()) ;
    QElapsedTimer t1;
    t1.start();
    while (t1.elapsed()<100) {
        QCoreApplication::processEvents();
    }

    close(m_iSerialPort);
    qDebug() << "CommCamera";
}

bool  CommCamera::SerialInit(bool bInited)
{
    qDebug() << "Init.";

    const char* m_cchardevice="/dev/corser/XtiumCLMX41_s0";//设置你的端口号
    m_iSerialPort = open(m_cchardevice, O_RDWR|O_NOCTTY|O_NDELAY);
    if(-1 == m_iSerialPort)
    {
        qDebug()<<"Open Serial Port Error!\n";
        m_bPortIsOpen = false;
        return false;
    }
    if( (fcntl(m_iSerialPort, F_SETFL, 0)) < 0 )
    {
        qDebug()<<"Fcntl F_SETFL Error!\n";
        m_bPortIsOpen = false;
        return false;
    }
    cfsetispeed(&stNew, B9600);//9600
    cfsetospeed(&stNew, B9600);
    stNew.c_cflag &= ~PARENB;
    stNew.c_cflag |= CS8;
    stNew.c_cflag &= ~CSTOPB;
    stNew.c_cflag &= ~CSIZE;
    stNew.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | ICRNL | INPCK | ISTRIP);
    stNew.c_lflag &=  ~(ICANON | ECHO | ECHOE | IEXTEN | ISIG);
    stNew.c_oflag &= ~OPOST;
    stNew.c_cc[VTIME] = 100;
    stNew.c_cc[VMIN] = 0;

    if (!bInited)
    {
        int nByte;
        QByteArray Initdata;

        /// RST SENSOR
        Initdata="\x01\x03\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(200);
        Initdata="\x01\x43\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::sleep(1);

        /// write register
        Initdata="\x01\x03\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(200);
        Initdata="\x01\x83\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::sleep(5);

        /// RST CLK_N
        Initdata="\x01\x03\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(200);
        Initdata="\x01\x03\x23";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::sleep(1);

        /// train
        Initdata="\x01\x03\x03";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(200);
        Initdata="\x01\x03\x0B";
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::msleep(10);
        nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
        QThread::sleep(1);
    }

    m_bPortIsOpen=true;

    qDebug() << "Init OK.";

    return true;
}

void CommCamera::StartGrab(bool bStart)
{
    if(m_bPortIsOpen){
        if(bStart){
            QByteArray m_badata="\x51\x47\x03";
            SendData(m_badata);
            qDebug() << "Start Grab.";
        }
        else {
            QByteArray m_badata="\x51\x07\x03";
            SendData(m_badata);
            qDebug() << "Stop Grab.";
        }
    }
}

bool CommCamera::SendData(QByteArray message)
{
    if (m_bPortIsOpen)
    {
        int nByte,iSendBytes;
        iSendBytes=message.length();
        char* pacTemp = new char[iSendBytes];
        memcpy(pacTemp, message.data(), iSendBytes * sizeof(char));
        nByte = write(m_iSerialPort, pacTemp ,iSendBytes) ;
        delete[] pacTemp;
        QElapsedTimer t1;
        t1.start();
        while (t1.elapsed()<100) {
            QCoreApplication::processEvents();
        }

        return true;
    }
    else
        return false;
}

//char* CommCamera::TranslateData(char RB, char ADD, char Data)
//{
//    int temp = Data << 8 | ADD;
//    m_ccommand[0] = (uchar)(temp << 4 | RB << 3 | 1);
//    m_ccommand[1] = (uchar)(((temp >> 4) << 2) | 3);
//    m_ccommand[2] = (uchar)(((temp >> 10) << 2) | 3);
//    return m_ccommand;
//}

char* CommCamera::TranslateData(char RB, char ADD, char Data)
{
    int temp = Data << 8 | ADD;
    m_ccommand[0] = (uchar)(temp << 4 | RB << 2 | 1);
    m_ccommand[1] = (uchar)(((temp >> 4) << 2) | 3);
    m_ccommand[2] = (uchar)(((temp >> 10) << 2) | 3);
    return m_ccommand;
}

int CommCamera::OnSetTimeOfExposure(QString buffer)
{
    qDebug() << "SetTimeOfExposure.";

    float fExposure = buffer.toFloat();
    if (fExposure < 1.3)
        fExposure = 1.3;

    int Exposure = fExposure/0.045;
//    int Exposure = fExposure/0.03747;

    char low = (char)Exposure;
    char media = (char)(Exposure >> 8);
    char high = (char)(Exposure >> 16);
//    char RB = 1;
    char RB = 2;

    char Command3[3] = { 0x51, 0x07, 0x03 };
    char Command4[3] = { 0x51, 0x47, 0x03 };

    QByteArray Command0 = QByteArray(TranslateData(RB, 0, low),3);
    SendData(Command0);
    QThread::msleep(5);
    QByteArray Command1 = QByteArray( TranslateData(RB, 1, media),3);
    SendData(Command1);
    QThread::msleep(5);
    QByteArray Command2 = QByteArray(TranslateData(RB, 2, high),3);
    SendData(Command2);
    QThread::msleep(5);

//    WriteCommand();

    return 0;
}

int CommCamera::OnSetGain(QString buffer)
{
    qDebug() << "SetGain.";

    float fGain = buffer.toFloat();
    if (fGain > 6.6)
        fGain = 6.6;
    else if (fGain < 1.0)
        fGain = 1.0;

//    fGain = fGain - floor(fGain) >= 0.5 ? floor(fGain) + 0.5 : floor(fGain);

//    char cGain = char((fGain - 1.0) / 0.5) * 2;

    char cGain = 14;

    char RB = 1;
    QByteArray Command0 = QByteArray(TranslateData(RB, 0x0F, (cGain << 4) | 0b00000010), 3);
    SendData(Command0);
    QThread::msleep(5);
    QByteArray Command1 = QByteArray(TranslateData(RB, 0x10, ((cGain & 0b00110000) >> 4) | 0b00110000), 3);
    SendData(Command1);
    QThread::msleep(5);
    QByteArray Command2 = QByteArray(TranslateData(RB, 0x25, (cGain & 0b00111111) << 1), 3);
    SendData(Command2);
    QThread::msleep(5);

//    WriteCommand();

    return 0;
}

void CommCamera::OnSetWindow(int line,int start)
{
    qDebug() << "SetWindow.";

    char line_low = (char)line;
    char line_high = (char)(line >> 8);
    char start_low = (char)start;
    char start_high = (char)(start >> 8);
//    char RB = 1;
    char RB = 2;
    if(line==m_pGParam->m_SGrabberData.iFullWidth)
    {
        QByteArray Command0 = QByteArray(TranslateData(RB, 4, start_low),3);
        SendData(Command0);
        QByteArray Command1 = QByteArray(TranslateData(RB, 5, start_high),3);
        SendData(Command1);
        QByteArray Command2 = QByteArray(TranslateData(RB, 6, line_low),3);
        SendData(Command2);
        QByteArray Command3 = QByteArray(TranslateData(RB, 7, line_high),3);
        SendData(Command3);
    }
    else if(line==m_pGParam->m_SGrabberData.iSubWidth)
    {
        QByteArray Command2 = QByteArray(TranslateData(RB, 6, line_low),3);
        SendData(Command2);
        QByteArray Command3 = QByteArray(TranslateData(RB, 7, line_high),3);
        SendData(Command3);
        QByteArray Command0 = QByteArray(TranslateData(RB, 4, start_low),3);
        SendData(Command0);
        QByteArray Command1 = QByteArray(TranslateData(RB, 5, start_high),3);
        SendData(Command1);
    }
//    qDebug() << "Comm a = " << start;
}

void CommCamera::OnQueryTemp(float &fTemp)
{
//    qDebug() << "QueryTemp.";

    if (!m_bQueryTemp)
    {
        char acTempPre[1000];
        read(m_iSerialPort, acTempPre, 98);
        m_bQueryTemp = true;
    }
    else
    {
        QByteArray m_badata="\x01\x03\x03\x01\x03\x43";
        SendData(m_badata);
        char acTemp[3];
        memset(acTemp, 0, 3);
        read(m_iSerialPort, acTemp, 3);

        unsigned int uiValue = (((((unsigned char)acTemp[0]) & 0x7F) << 8)  + (((unsigned char)acTemp[1]) & 0xF8)) >> 3;
        if ((unsigned int)acTemp[0] < 128)
//            fTemp = float(uiValue) / 10 - 126;  // 姚安
            fTemp = float(uiValue) / 10 - 75;   // 订购3
        else
//            fTemp = (float(uiValue) - 4096) / 10 - 126; // 姚安
            fTemp = (float(uiValue) - 4096) / 10 - 75; // 订购3

//        qDebug() << "Serial:" << QString::number((unsigned char)acTemp[0]) << QString::number((unsigned char)acTemp[1]) << QString::number((unsigned char)acTemp[2]);
    }
}

void CommCamera::OnTrainCam()
{
    int nByte;
    QByteArray Initdata;
    /// train
    Initdata="\x01\x03\x03";
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::msleep(10);
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::msleep(10);
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::msleep(200);
    Initdata="\x01\x03\x0B";
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::msleep(10);
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::msleep(10);
    nByte = write(m_iSerialPort, Initdata.data() ,Initdata.length()) ;
    QThread::sleep(1);
}



