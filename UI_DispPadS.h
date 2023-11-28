#ifndef UI_DISPPADS_H
#define UI_DISPPADS_H

#include <QDialog>
#include <QPainter>
#include "QImage8UC1BufferPackage.h"
#include "GlobalParameter.h"
#include "ImageProcessor.h"

namespace Ui {
class UI_DispPadS;
}

class UI_DispPadS : public QDialog
{
    Q_OBJECT

public:
    explicit UI_DispPadS(QWidget *parent = nullptr);
    ~UI_DispPadS();

protected:
    void keyPressEvent(QKeyEvent* event);

private:
    void paintImage(void);

private slots:
    void on_SignalDisplay(void);
    void on_SignalClose(void);
    void on_SignalCropDisp(bool);

private:
    Ui::UI_DispPadS *ui;
    GlobalParameter* m_pGParam;	// 全局参数对象指针
    ImageProcessor* m_pImageProcessor;	// 图像处理对象指针
};

#endif // UI_DISPPADS_H
