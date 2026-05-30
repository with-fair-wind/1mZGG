#include "UI_DispPadS.h"
#include "ui_UI_DispPadS.h"

UI_DispPadS::UI_DispPadS(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UI_DispPadS)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pImageProcessor = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
    setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint);
    ui->setupUi(this);
    setFixedSize(this->width(), this->height());
    setGeometry(1000, 500, this->width(), this->height());
}

UI_DispPadS::~UI_DispPadS()
{
    delete ui;
}

void UI_DispPadS::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Escape:
            event->ignore();
            break;
        case Qt::Key_Enter:
            event->ignore();
            break;
        case Qt::Key_Space:
            event->ignore();
            break;
        default:
            QDialog::keyPressEvent(event);
    }
}

void UI_DispPadS::on_SignalClose()
{
    close();
}

void UI_DispPadS::on_SignalCropDisp(bool bDisp)
{
    if (bDisp)
        show();
    else
        hide();
}

void UI_DispPadS::paintImage()
{
    unsigned int uiWidth, uiHeight;
    unsigned char* paucDispImage = m_pImageProcessor->GetDispImage(uiWidth, uiHeight);
    if (paucDispImage
        && uiWidth == m_pGParam->m_SGrabberData.iFullWidth
        && uiHeight == m_pGParam->m_SGrabberData.iFullHeight)
    {
        QImage8UC1BufferPackage qimgDisp(paucDispImage, uiWidth, uiHeight);
        QRect qROI(uiWidth/2 - ui->QImage->width()/2,
                   uiHeight/2 - ui->QImage->height()/2,
                   ui->QImage->width(),
                   ui->QImage->height());
        QImage qimgDispCrop = qimgDisp.copy(qROI);
        QPixmap qpixmapDraw = QPixmap::fromImage(qimgDispCrop);
        ui->QImage->setPixmap(qpixmapDraw);
    }
}

void UI_DispPadS::on_SignalDisplay()
{
    paintImage();
}
