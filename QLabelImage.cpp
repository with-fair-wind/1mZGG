#include "QLabelImage.h"

QLabelImage::QLabelImage(QWidget *parent) : QLabel(parent)
{
    setMouseTracking(true);
}

QLabelImage::~QLabelImage()
{
}

void QLabelImage::mouseMoveEvent(QMouseEvent* event)
{
    emit LabelMouseMove(event->pos());
//    qDebug() << "Mouse Pos: (" << event->pos().x() << ", " << event->pos().y() << ")." << endl;
}

void QLabelImage::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        emit LabelRightDoubleClicked(event->pos());
    else if(event->button() == Qt::LeftButton)
        emit LabelMouseDoubleClicked(event->pos());
    //    qDebug() << "Double Clicked Pos: (" << event->pos().x() << ", " << event->pos().y() << ")." << endl;
}

void QLabelImage::mousePressEvent(QMouseEvent *event)
{
    emit LabelMouseClicked(event->pos());
}

