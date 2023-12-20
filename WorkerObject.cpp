#include "WorkerObject.h"

WorkerObject::WorkerObject(QObject *parent) : QObject(parent)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_qthdRun = new QThread;
    moveToThread(m_qthdRun);
    m_qthdRun->start();
}

void WorkerObject::doPythonWork(QString pythonScript, QStringList functionArguments)
{
    QProcess pythonProcess;
    pythonProcess.start(m_pGParam->m_STrackParams.qstrExEPath, QStringList() << pythonScript << functionArguments);
    if (!pythonProcess.waitForStarted())
        qDebug() << "Failed to start Python process.";
    if (!pythonProcess.waitForFinished())
        qDebug() << "Failed to finish Python process.";
    QString output = pythonProcess.readAllStandardOutput();
    qDebug() << "Python output:" << output;
    emit SignalPyEnding();
}
