#pragma once

#include "dss/ui/image_display.h"

namespace Dss::Ui
{

class ImageDisplayCrop : public ImageDisplay
{
    Q_OBJECT

public:
    explicit ImageDisplayCrop(QWidget* parent = nullptr);

    void setCropCenter(QPointF center);
    void setCropSize(int size);

private:
    QPointF m_cropCenter{};
    int m_cropSize = 128;
};

} // namespace Dss::Ui
