#include "dss/ui/image_display.h"

#include <QPainter>

namespace Dss::Ui {

ImageDisplay::ImageDisplay(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(320, 240);
}

// 同尺寸图像保留当前视口；空图像或尺寸变化时重置视图。
void ImageDisplay::setImage(const QImage& image) {
    const auto keepViewport = m_viewport.imageSize() == image.size() && !image.isNull();
    m_currentImage = image;
    m_viewport.setViewportSize(size());
    m_viewport.setImageSize(m_currentImage.size(), keepViewport);
    update();
}

/// 将缩放与偏移重置为适应窗口并居中
void ImageDisplay::resetView() {
    m_viewport.setViewportSize(size());
    m_viewport.reset();
    update();
}

auto ImageDisplay::imagePositionAt(const QPointF& widgetPos) const -> QPointF {
    return m_viewport.widgetToImage(widgetPos);
}

auto ImageDisplay::imageScaleFactor() const -> double {
    return m_viewport.scaleFactor();
}

auto ImageDisplay::imageOffset() const -> QPointF {
    return m_viewport.offset();
}

// 仅绘制与视口相交的图像片段以提升大图性能。
void ImageDisplay::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!m_currentImage.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, m_viewport.scaleFactor() < 1.0);
        const auto sourceRect = m_viewport.visibleImageRect();
        if (!sourceRect.isEmpty()) {
            const QRectF targetRect(m_viewport.imageToWidget(sourceRect.topLeft()),
                                    sourceRect.size() * m_viewport.scaleFactor());
            painter.drawImage(targetRect, m_currentImage, sourceRect);
        }
    }
}

void ImageDisplay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && !m_currentImage.isNull()) {
        m_isPanning = true;
        m_lastPanPosition = event->position();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto imgPos = imagePositionAt(event->position());
        Q_EMIT positionClicked(imgPos);
    }
    QWidget::mousePressEvent(event);
}

void ImageDisplay::mouseMoveEvent(QMouseEvent* event) {
    const auto isMiddleDrag = m_isPanning && event->buttons().testFlag(Qt::MiddleButton);
    if (isMiddleDrag) {
        const auto delta = event->position() - m_lastPanPosition;
        m_lastPanPosition = event->position();
        m_viewport.panBy(delta);
        update();
    }

    const auto imgPos = imagePositionAt(event->position());
    Q_EMIT mouseMoved(imgPos);
    if (isMiddleDrag) {
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ImageDisplay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// 窗口尺寸变化时保持视口中心对应的图像点不变。
void ImageDisplay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!m_currentImage.isNull()) {
        m_viewport.resizeKeepingCenter(event->oldSize(), event->size());
        update();
    }
}

// 以鼠标位置为锚点进行滚轮缩放。
void ImageDisplay::wheelEvent(QWheelEvent* event) {
    if (m_currentImage.isNull() || event->angleDelta().y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    m_viewport.setViewportSize(size());
    (void)m_viewport.zoomAt(event->position(), event->angleDelta().y());
    update();
    event->accept();
}

}  // namespace Dss::Ui
