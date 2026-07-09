#pragma once

#include <QDoubleSpinBox>
#include <QSpinBox>

class QWheelEvent;

namespace Dss::Ui {

/// @brief 仅在拥有键盘焦点时响应滚轮的整数输入框。
class WheelGuardedSpinBox final : public QSpinBox {
public:
    /**
     * @brief 创建滚轮保护整数输入框。
     * @param parent Qt 父控件。
     */
    explicit WheelGuardedSpinBox(QWidget* parent = nullptr);

protected:
    /**
     * @brief 有焦点时修改数值，否则把滚轮事件交回父级滚动区域。
     * @param event 滚轮事件。
     */
    void wheelEvent(QWheelEvent* event) override;
};

/// @brief 仅在拥有键盘焦点时响应滚轮的浮点输入框。
class WheelGuardedDoubleSpinBox final : public QDoubleSpinBox {
public:
    /**
     * @brief 创建滚轮保护浮点输入框。
     * @param parent Qt 父控件。
     */
    explicit WheelGuardedDoubleSpinBox(QWidget* parent = nullptr);

protected:
    /**
     * @brief 有焦点时修改数值，否则把滚轮事件交回父级滚动区域。
     * @param event 滚轮事件。
     */
    void wheelEvent(QWheelEvent* event) override;
};

}  // namespace Dss::Ui
