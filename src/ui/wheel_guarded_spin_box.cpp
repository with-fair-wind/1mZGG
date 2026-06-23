#include "dss/ui/wheel_guarded_spin_box.h"

#include <QWheelEvent>

namespace Dss::Ui {
WheelGuardedSpinBox::WheelGuardedSpinBox(QWidget* parent) : QSpinBox(parent) {
    setFocusPolicy(Qt::StrongFocus);
}

WheelGuardedDoubleSpinBox::WheelGuardedDoubleSpinBox(QWidget* parent) : QDoubleSpinBox(parent) {
    setFocusPolicy(Qt::StrongFocus);
}

void WheelGuardedSpinBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus()) {
        event->ignore();
        return;
    }
    QSpinBox::wheelEvent(event);
}

void WheelGuardedDoubleSpinBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus()) {
        event->ignore();
        return;
    }
    QDoubleSpinBox::wheelEvent(event);
}

}  // namespace Dss::Ui
