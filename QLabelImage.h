#ifndef QLABELIMAGE_H
#define QLABELIMAGE_H


#include <QLabel>
#include <QPoint>
#include <QMouseEvent>
#include <QDebug>

class QLabelImage : public QLabel
{
Q_OBJECT

public:
    QLabelImage(QWidget *parent = nullptr);
    ~QLabelImage();

protected:
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);

signals:
    void LabelMouseMove(QPoint qptPos);
    void LabelMouseDoubleClicked(QPoint qptPos);
    void LabelRightDoubleClicked(QPoint qptPos);
    void LabelMouseClicked(QPoint qptPos);
};

#endif // QLABELIMAGE_H
