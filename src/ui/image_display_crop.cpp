#include "dss/ui/image_display_crop.h"

namespace Dss::Ui {

ImageDisplayCrop::ImageDisplayCrop(QWidget* parent) : ImageDisplay(parent) {
    setMinimumSize(256, 256);
}

void ImageDisplayCrop::setCropCenter(QPointF center) {
    m_cropCenter = center;
}

void ImageDisplayCrop::setCropSize(int size) {
    m_cropSize = size;
}

}  // namespace Dss::Ui
