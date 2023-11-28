#ifndef MYLOG_H
#define MYLOG_H

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include "GlobalParameter.h"

class MyLog : public QObject
{
	Q_OBJECT

/// 函数
public:
	MyLog();
	~MyLog();
	void DebugMsg(QString qstrMsg);
	void InfoMsg(QString qstrMsg);
	void WarnMsg(QString qstrMsg);
	void CriticalMsg(QString qstrMsg);
	void FatalMsg(QString qstrMsg);

private:
	void GenerateMsg(int iMsgType, QString qstrMsg);

signals:
	void SignalMsgPrint(QString qstrMsg);

private:
	GlobalParameter* m_pGParam;
};


#endif // MYLOG_H
