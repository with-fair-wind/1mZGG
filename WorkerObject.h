#ifndef WORKEROBJECT_H
#define WORKEROBJECT_H

#include <QObject>
#include <QProcess>
#include <QDebug>
#include <QThread>
#include "GlobalParameter.h"

class WorkerObject : public QObject
{
    Q_OBJECT
public:
    explicit WorkerObject(QObject *parent = nullptr);

signals:
    void SignalPyEnding();

public slots:
    void doPythonWork(QString pythonScript, QStringList functionArguments);

private:
    GlobalParameter* m_pGParam;
    QThread* m_qthdRun;
};

#endif // WORKEROBJECT_H
