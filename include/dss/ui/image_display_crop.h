#pragma once

#include "dss/ui/image_display.h"

namespace Dss::Ui {

/// 带裁剪区域参数的图像显示控件，用于局部放大显示
class ImageDisplayCrop : public ImageDisplay {
    Q_OBJECT

public:
    explicit ImageDisplayCrop(QWidget* parent = nullptr);

    /// 设置裁剪区域中心（图像坐标）
    void setCropCenter(QPointF center);
    /// 设置裁剪区域边长（像素）
    void setCropSize(int size);

private:
    QPointF m_cropCenter{}; ///< 裁剪区域中心（图像坐标）
    int m_cropSize = 128; ///< 裁剪区域边长（像素）
};

}  // namespace Dss::Ui
