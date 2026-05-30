#ifndef QIMAGE8UC1BUFFERPACKAGE_H
#define QIMAGE8UC1BUFFERPACKAGE_H


#include <QImage>
#include <QVector>

class QImage8UC1BufferPackage : public QImage
{
/// 函数
public:
    QImage8UC1BufferPackage(const unsigned char* cpaucBuffer, int iImageWidth, int iImageHeight);
    ~QImage8UC1BufferPackage(void);

/// 变量
private:
    QVector<QRgb> m_qvectorColorTable;
    unsigned char* m_paucBuffer;
};
#endif // QIMAGE8UC1BUFFERPACKAGE_H
