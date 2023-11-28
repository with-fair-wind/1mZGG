#ifndef UI_DISTCURVE_H
#define UI_DISTCURVE_H

#include <QDialog>

namespace Ui {
class UI_DistCurve;
}

class UI_DistCurve : public QDialog
{
    Q_OBJECT

public:
    explicit UI_DistCurve(QWidget *parent = 0);
    ~UI_DistCurve();

protected:
    void keyPressEvent(QKeyEvent* event);
    void closeEvent(QCloseEvent* event);

private:
    Ui::UI_DistCurve *ui;
};

#endif // UI_DISTCURVE_H
