#include "MyLog.h"

MyLog::MyLog()
{
	m_pGParam = GlobalParameter::GetInstance();
	m_pGParam->m_SLog.pvoidThis = (void*)this;

    InfoMsg("Power On");
}
#include <QDebug>
MyLog::~MyLog()
{
    InfoMsg("Power Off");
    qDebug() << "MyLog";
}

void MyLog::DebugMsg(QString qstrMsg)
{
	GenerateMsg(0, qstrMsg);
}

void MyLog::InfoMsg(QString qstrMsg)
{
	GenerateMsg(1, qstrMsg);
}

void MyLog::WarnMsg(QString qstrMsg)
{
	GenerateMsg(2, qstrMsg);
}

void MyLog::CriticalMsg(QString qstrMsg)
{
	GenerateMsg(3, qstrMsg);
}

void MyLog::FatalMsg(QString qstrMsg)
{
	GenerateMsg(4, qstrMsg);
}

void MyLog::GenerateMsg(int iMsgType, QString qstrMsg)
{
	QString qstrMsgType = "";
	switch (iMsgType)
	{
	case 0:
        qstrMsgType = QStringLiteral(" Debug");
		break;
	case 1:
        qstrMsgType = QStringLiteral(" Info");
		break;
	case 2:
        qstrMsgType = QStringLiteral(" Warn");
		break;
	case 3:
        qstrMsgType = QStringLiteral(" Error");
		break;
	case 4:
        qstrMsgType = QStringLiteral(" Fatal");
		break;
	default:
		break;
	}
	QDateTime qdatetimeCurrent = QDateTime::currentDateTime();
    QString qstrLogPath = m_pGParam->m_SPath.qstrDataStorePath;
    qstrLogPath = qstrLogPath.left(qstrLogPath.size()-4) + "LOGS";
    QDir dir;
    if (!dir.exists(qstrLogPath))
    {
        dir.mkdir(qstrLogPath);
    }
    QString qstrLogFile = qstrLogPath + "/" + qdatetimeCurrent.toString("yyyyMMdd_DPS.log");

	QString qstrLogMsg = qdatetimeCurrent.toString("[yyyy-MM-dd hh:mm:ss.zzz] ");
	qstrLogMsg += qstrMsgType;
	qstrLogMsg += ":{ ";
	qstrLogMsg += qstrMsg;
	qstrLogMsg += " }";
	QFile qfileLog(qstrLogFile);
	qfileLog.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append);
	QTextStream qtsLog(&qfileLog);
	qtsLog << qstrLogMsg << endl;
	qfileLog.close();
	emit SignalMsgPrint(qstrLogMsg);
}
