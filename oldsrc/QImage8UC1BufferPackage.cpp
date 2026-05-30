#include "QImage8UC1BufferPackage.h"

QImage8UC1BufferPackage::QImage8UC1BufferPackage(const unsigned char* cpaucBuffer, int iImageWidth, int iImageHeight)
        : QImage((iImageWidth + 3) / 4 * 4, iImageHeight, QImage::Format_Indexed8)
{
    for (int i = 0; i < 256; i++)
    {
        m_qvectorColorTable.append(qRgb(i, i, i));
    }
    m_paucBuffer = new unsigned char[iImageWidth * iImageHeight];
    memcpy(m_paucBuffer, cpaucBuffer, iImageWidth * iImageHeight);
    int iLineByte = (iImageWidth + 3) / 4 * 4;
    memset(this->bits(), 0, iLineByte * iImageHeight);
    for (int i = 0; i < iImageHeight; i++)
    {
        memcpy(this->bits() + i * iLineByte, m_paucBuffer + i * iImageWidth, iImageWidth);
    }
    this->setColorTable(m_qvectorColorTable);
}

QImage8UC1BufferPackage::~QImage8UC1BufferPackage()
{
    if (m_paucBuffer != NULL)
    {
        delete[] m_paucBuffer;
    }
}
