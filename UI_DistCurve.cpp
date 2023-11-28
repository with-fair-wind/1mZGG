#include "UI_DistCurve.h"
#include "ui_UI_DistCurve.h"
#include <QKeyEvent>

UI_DistCurve::UI_DistCurve(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UI_DistCurve)
{
    ui->setupUi(this);
}

UI_DistCurve::~UI_DistCurve()
{
    delete ui;
}

void UI_DistCurve::keyPressEvent(QKeyEvent* event)
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

void UI_DistCurve::closeEvent(QCloseEvent* event)
{
    delete this;
}
