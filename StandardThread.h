#ifndef STANDARDTHREAD_H
#define STANDARDTHREAD_H


#include <QThread>

class StandardThread : public QThread
{
Q_OBJECT

public:
    StandardThread(void (*pRun)(void*), void* pCaller);
    ~StandardThread();
protected:
    void run();

private:
    void (*m_pRun)(void*);
    void* m_pCaller;

public:
    bool m_stopRequested;


};

#endif // STANDARDTHREAD_H
