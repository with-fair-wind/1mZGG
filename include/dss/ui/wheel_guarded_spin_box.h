#pragma once

#include <QDoubleSpinBox>
#include <QSpinBox>

class QWheelEvent;

namespace Dss::Ui {

class WheelGuardedSpinBox final : public QSpinBox {
public:
    explicit WheelGuardedSpinBox(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

class WheelGuardedDoubleSpinBox final : public QDoubleSpinBox {
public:
    explicit WheelGuardedDoubleSpinBox(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

}  // namespace Dss::Ui
