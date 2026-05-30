#include "StandardThread.h"

StandardThread::StandardThread(void(*pRun)(void*), void* pCaller)
{
    m_pRun = pRun;
    m_pCaller = pCaller;
    m_stopRequested = false;
}

StandardThread::~StandardThread()
{
    m_stopRequested = true; // Assuming m_stopRequested is a boolean flag
    if (isRunning())
        wait();
}

void StandardThread::run()
{
//    while (!m_stopRequested)
        m_pRun(m_pCaller);
}
