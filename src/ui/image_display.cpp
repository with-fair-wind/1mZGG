#include "dss/ui/image_display.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace Dss::Ui {

namespace {

constexpr auto kWheelZoomStep = 1.25;
constexpr auto kMaxImageScaleFactor = 32.0;

}  // namespace

ImageDisplay::ImageDisplay(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(320, 240);
}

/**
 * @brief 更新显示图像
 * 同尺寸图像保留当前视口；空图像或尺寸变化时重置视图
 * @param image 待显示图像
 */
void ImageDisplay::setImage(const QImage& image) {
    const auto keepViewport =
        !m_currentImage.isNull() && m_currentImage.size() == image.size() && !image.isNull();
    m_currentImage = image;
    if (m_currentImage.isNull()) {
        m_scaleFactor = 1.0;
        m_offset = {};
    } else if (keepViewport) {
        clampOffset();
    } else {
        resetView();
    }
    update();
}

/// 将缩放与偏移重置为适应窗口并居中
void ImageDisplay::resetView() {
    m_scaleFactor = fitScale();
    const auto scaledWidth = static_cast<double>(m_currentImage.width()) * m_scaleFactor;
    const auto scaledHeight = static_cast<double>(m_currentImage.height()) * m_scaleFactor;
    m_offset = QPointF((static_cast<double>(width()) - scaledWidth) / 2.0,
                       (static_cast<double>(height()) - scaledHeight) / 2.0);
    clampOffset();
    update();
}

auto ImageDisplay::imagePositionAt(const QPointF& widgetPos) const -> QPointF {
    if (m_scaleFactor <= 0.0) {
        return {};
    }
    return {(widgetPos.x() - m_offset.x()) / m_scaleFactor,
            (widgetPos.y() - m_offset.y()) / m_scaleFactor};
}

auto ImageDisplay::imageScaleFactor() const -> double {
    return m_scaleFactor;
}

auto ImageDisplay::imageOffset() const -> QPointF {
    return m_offset;
}

/**
 * @brief 绘制可见区域内的图像片段
 * 仅绘制与视口相交的部分以提升大图性能
 */
void ImageDisplay::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!m_currentImage.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, m_scaleFactor < 1.0);
        const QRectF imageBounds(QPointF(0.0, 0.0), QSizeF(m_currentImage.size()));
        auto sourceRect = QRectF(imagePositionAt(QPointF(0.0, 0.0)),
                                 imagePositionAt(QPointF(static_cast<double>(width()),
                                                         static_cast<double>(height()))))
                              .normalized()
                              .intersected(imageBounds);
        if (!sourceRect.isEmpty()) {
            const QRectF targetRect(imageToWidget(sourceRect.topLeft()),
                                    sourceRect.size() * m_scaleFactor);
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
        m_offset += delta;
        clampOffset();
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

/**
 * @brief 窗口尺寸变化时保持视口中心对应的图像点不变
 * @param event 尺寸变化事件
 */
void ImageDisplay::resizeEvent(QResizeEvent* event) {
    const auto oldSize = event->oldSize();
    const auto hadOldSize = oldSize.isValid() && oldSize.width() > 0 && oldSize.height() > 0;
    const auto oldCenter =
        hadOldSize ? imagePositionAt(QPointF(static_cast<double>(oldSize.width()) / 2.0,
                                             static_cast<double>(oldSize.height()) / 2.0))
                   : QPointF(static_cast<double>(m_currentImage.width()) / 2.0,
                             static_cast<double>(m_currentImage.height()) / 2.0);

    QWidget::resizeEvent(event);
    if (!m_currentImage.isNull()) {
        const auto minScale = fitScale();
        m_scaleFactor = std::max(m_scaleFactor, minScale);
        m_offset = QPointF(static_cast<double>(width()) / 2.0 - oldCenter.x() * m_scaleFactor,
                           static_cast<double>(height()) / 2.0 - oldCenter.y() * m_scaleFactor);
        clampOffset();
        update();
    }
}

/**
 * @brief 以鼠标位置为锚点进行滚轮缩放
 * @param event 滚轮事件
 */
void ImageDisplay::wheelEvent(QWheelEvent* event) {
    if (m_currentImage.isNull() || event->angleDelta().y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const auto anchor = imagePositionAt(event->position());
    const auto multiplier =
        std::pow(kWheelZoomStep, static_cast<double>(event->angleDelta().y()) / 120.0);
    const auto minScale = fitScale();
    const auto maxScale = std::max(kMaxImageScaleFactor, minScale);
    const auto nextScale = std::clamp(m_scaleFactor * multiplier, minScale, maxScale);

    if (std::abs(nextScale - m_scaleFactor) <= 1.0e-12) {
        event->accept();
        return;
    }

    m_scaleFactor = nextScale;
    m_offset = QPointF(event->position().x() - anchor.x() * m_scaleFactor,
                       event->position().y() - anchor.y() * m_scaleFactor);
    clampOffset();
    update();
    event->accept();
}

/// 计算使整幅图像适应控件的最小等比缩放
auto ImageDisplay::fitScale() const -> double {
    if (m_currentImage.isNull() || width() <= 0 || height() <= 0 || m_currentImage.width() <= 0 ||
        m_currentImage.height() <= 0) {
        return 1.0;
    }
    const auto scaleX = static_cast<double>(width()) / static_cast<double>(m_currentImage.width());
    const auto scaleY =
        static_cast<double>(height()) / static_cast<double>(m_currentImage.height());
    return std::min(scaleX, scaleY);
}

auto ImageDisplay::imageToWidget(const QPointF& imagePos) const -> QPointF {
    return {imagePos.x() * m_scaleFactor + m_offset.x(),
            imagePos.y() * m_scaleFactor + m_offset.y()};
}

/// 将缩放比例与偏移限制在合法范围，小图居中、大图边缘不外露
void ImageDisplay::clampOffset() {
    if (m_currentImage.isNull()) {
        return;
    }

    const auto minScale = fitScale();
    const auto maxScale = std::max(kMaxImageScaleFactor, minScale);
    m_scaleFactor = std::clamp(m_scaleFactor, minScale, maxScale);

    const auto scaledWidth = static_cast<double>(m_currentImage.width()) * m_scaleFactor;
    const auto scaledHeight = static_cast<double>(m_currentImage.height()) * m_scaleFactor;
    const auto viewportWidth = static_cast<double>(width());
    const auto viewportHeight = static_cast<double>(height());

    if (scaledWidth <= viewportWidth) {
        m_offset.setX((viewportWidth - scaledWidth) / 2.0);
    } else {
        m_offset.setX(std::clamp(m_offset.x(), viewportWidth - scaledWidth, 0.0));
    }

    if (scaledHeight <= viewportHeight) {
        m_offset.setY((viewportHeight - scaledHeight) / 2.0);
    } else {
        m_offset.setY(std::clamp(m_offset.y(), viewportHeight - scaledHeight, 0.0));
    }
}

}  // namespace Dss::Ui
