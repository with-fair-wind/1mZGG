#ifndef UI_DISPPAD_H
#define UI_DISPPAD_H

#include <QDialog>
#include <QPainter>
#include <QDateTime>
#include <QImage>
#include <math.h>
#include "QLabelImage.h"
#include "QImage8UC1BufferPackage.h"
#include "GlobalParameter.h"
#include "ImageProcessor.h"
#include "Grabber.h"
//#include "DataProcessor.h"

namespace Ui {
class UI_DispPad;
}

class UI_DispPad : public QDialog
{
    Q_OBJECT

public:
    explicit UI_DispPad(QWidget *parent = nullptr);
    ~UI_DispPad();
    void painttext();
    void paintImage();
    double d_size;

protected:
    void keyPressEvent(QKeyEvent* event) override;

    void resizeEvent(QResizeEvent *event) override;

//    bool event(QEvent *event) override;

private slots:
    void on_SignalDisplay(void);
    void on_SignalClose(void);
    void on_SignalDrawStarMap(bool);
    void on_LabelMouseMove(QPoint qptPos);
    void on_LabelMouseDoubleClicked(QPoint qptPos);
    void on_LabelMouseClicked(QPoint qptPos);
    void on_LabelRightDoubleClicked(QPoint qptPos);
    void on_SignalZoom(int iZoom);

signals:
    void SignalManualInit(void);
    void SignalLabelMouseClicked(float fClickX, float fClickY);
    void SignalCropDisp(bool);


private:
    Ui::UI_DispPad *ui;
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    ImageProcessor* m_pImageProcessor;	// 图像处理对象指针
//    DataProcessor* m_pDataProcessor;
    Grabber* m_pGrabber;
    QImage m_pqimgScale;
    uint m_uiImageWidth;
    uint m_uiImageHeight;
    bool m_bDrawStarMap;
    bool m_bCropDisp;
    QPointF m_qptLUinImage;
    QPointF m_qptRDinImage;
    QPointF m_qptRightClickinImage;
    unsigned int m_uiZoom;
    unsigned int m_uiZoomCtrl;

    unsigned m_side;
    double m_dRatio;
    bool m_bHeightLonger; // true : height ; false : width
};

#endif // UI_DISPPAD_H



