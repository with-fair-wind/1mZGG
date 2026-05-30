#include "dss/ui/image_display.h"

#include <QPainter>

namespace Dss::Ui
{

ImageDisplay::ImageDisplay(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(320, 240);
}

void ImageDisplay::setImage(const QImage& image)
{
    m_currentImage = image;
    if (!m_currentImage.isNull())
    {
        m_scaledImage = m_currentImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_scaleFactor = static_cast<double>(m_scaledImage.width()) / m_currentImage.width();
        m_offset = QPointF(
            (width() - m_scaledImage.width()) / 2.0,
            (height() - m_scaledImage.height()) / 2.0);
    }
    update();
}

void ImageDisplay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!m_scaledImage.isNull())
    {
        painter.drawImage(m_offset, m_scaledImage);
    }
}

void ImageDisplay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        auto imgPos = widgetToImage(event->position());
        Q_EMIT positionClicked(imgPos);
    }
    QWidget::mousePressEvent(event);
}

void ImageDisplay::mouseMoveEvent(QMouseEvent* event)
{
    auto imgPos = widgetToImage(event->position());
    Q_EMIT mouseMoved(imgPos);
    QWidget::mouseMoveEvent(event);
}

void ImageDisplay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_currentImage.isNull())
    {
        setImage(m_currentImage);
    }
}

QPointF ImageDisplay::widgetToImage(const QPointF& widgetPos) const
{
    if (m_scaleFactor <= 0.0)
    {
        return {};
    }
    return QPointF(
        (widgetPos.x() - m_offset.x()) / m_scaleFactor,
        (widgetPos.y() - m_offset.y()) / m_scaleFactor);
}

} // namespace Dss::Ui
