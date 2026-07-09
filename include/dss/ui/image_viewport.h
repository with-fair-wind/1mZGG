#pragma once

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <algorithm>
#include <cmath>

namespace Dss::Ui {

/**
 * @brief 图像显示视口状态与坐标变换工具。
 *
 * 该类封装 CPU 与 GPU 图像控件共享的缩放、平移、边界约束和坐标转换逻辑。
 * 它不负责绘制，也不依赖具体 QWidget 类型。
 */
class ImageViewport {
public:
    /** @brief 滚轮缩放的单格倍率。 */
    static constexpr double kWheelZoomStep = 1.25;
    /** @brief 默认最大显示倍率。 */
    static constexpr double kMaxImageScaleFactor = 32.0;

    /**
     * @brief 设置控件视口尺寸。
     * @param viewportSize 控件可显示区域尺寸。
     */
    void setViewportSize(QSize viewportSize) {
        m_viewportSize = viewportSize;
        clamp();
    }

    /**
     * @brief 设置图像尺寸。
     * @param imageSize 当前图像尺寸。
     * @param keepViewport 图像尺寸不变时是否保留现有视口。
     */
    void setImageSize(QSize imageSize, bool keepViewport) {
        m_imageSize = imageSize;
        if (m_imageSize.isEmpty()) {
            clear();
        } else if (keepViewport) {
            clamp();
        } else {
            reset();
        }
    }

    /** @brief 清空视口状态。 */
    void clear() {
        m_imageSize = {};
        m_scaleFactor = 1.0;
        m_offset = {};
    }

    /** @brief 将视图重置为完整图像适配视口并居中。 */
    void reset() {
        m_scaleFactor = fitScale();
        const auto scaledWidth = static_cast<double>(m_imageSize.width()) * m_scaleFactor;
        const auto scaledHeight = static_cast<double>(m_imageSize.height()) * m_scaleFactor;
        m_offset = QPointF((static_cast<double>(m_viewportSize.width()) - scaledWidth) / 2.0,
                           (static_cast<double>(m_viewportSize.height()) - scaledHeight) / 2.0);
        clamp();
    }

    /**
     * @brief 控件尺寸变化时保持原中心对应的图像点不变。
     * @param oldViewportSize 变化前的视口尺寸。
     * @param newViewportSize 变化后的视口尺寸。
     */
    void resizeKeepingCenter(QSize oldViewportSize, QSize newViewportSize) {
        const auto hadOldSize = oldViewportSize.isValid() && oldViewportSize.width() > 0 &&
                                oldViewportSize.height() > 0;
        const auto oldCenter =
            hadOldSize ? widgetToImage(QPointF(static_cast<double>(oldViewportSize.width()) / 2.0,
                                               static_cast<double>(oldViewportSize.height()) / 2.0))
                       : QPointF(static_cast<double>(m_imageSize.width()) / 2.0,
                                 static_cast<double>(m_imageSize.height()) / 2.0);

        m_viewportSize = newViewportSize;
        if (!m_imageSize.isEmpty()) {
            m_scaleFactor = std::max(m_scaleFactor, fitScale());
            m_offset = QPointF(
                static_cast<double>(m_viewportSize.width()) / 2.0 - oldCenter.x() * m_scaleFactor,
                static_cast<double>(m_viewportSize.height()) / 2.0 - oldCenter.y() * m_scaleFactor);
            clamp();
        }
    }

    /**
     * @brief 按控件坐标中的位移平移视口。
     * @param delta 控件坐标中的拖拽位移。
     */
    void panBy(const QPointF& delta) {
        if (m_imageSize.isEmpty()) {
            return;
        }
        m_offset += delta;
        clamp();
    }

    /**
     * @brief 以控件坐标中的锚点执行滚轮缩放。
     * @param anchorWidgetPos 缩放锚点，通常为鼠标位置。
     * @param angleDeltaY Qt 滚轮垂直角度增量。
     * @return 缩放事件被处理时返回 true。
     */
    [[nodiscard]] auto zoomAt(const QPointF& anchorWidgetPos, int angleDeltaY) -> bool {
        if (m_imageSize.isEmpty() || angleDeltaY == 0) {
            return false;
        }

        const auto anchor = widgetToImage(anchorWidgetPos);
        const auto multiplier = std::pow(kWheelZoomStep, static_cast<double>(angleDeltaY) / 120.0);
        const auto minScale = fitScale();
        const auto maxScale = std::max(kMaxImageScaleFactor, minScale);
        m_scaleFactor = std::clamp(m_scaleFactor * multiplier, minScale, maxScale);
        m_offset = QPointF(anchorWidgetPos.x() - anchor.x() * m_scaleFactor,
                           anchorWidgetPos.y() - anchor.y() * m_scaleFactor);
        clamp();
        return true;
    }

    /**
     * @brief 将控件坐标转换为图像坐标。
     * @param widgetPos 控件内坐标。
     * @return 当前视口下对应的图像坐标。
     */
    [[nodiscard]] auto widgetToImage(const QPointF& widgetPos) const -> QPointF {
        if (m_scaleFactor <= 0.0) {
            return {};
        }
        return {(widgetPos.x() - m_offset.x()) / m_scaleFactor,
                (widgetPos.y() - m_offset.y()) / m_scaleFactor};
    }

    /**
     * @brief 将图像坐标转换为控件坐标。
     * @param imagePos 图像坐标。
     * @return 当前视口下对应的控件坐标。
     */
    [[nodiscard]] auto imageToWidget(const QPointF& imagePos) const -> QPointF {
        return {imagePos.x() * m_scaleFactor + m_offset.x(),
                imagePos.y() * m_scaleFactor + m_offset.y()};
    }

    /**
     * @brief 获取当前可见图像区域。
     * @return 与图像范围相交后的可见图像矩形。
     */
    [[nodiscard]] auto visibleImageRect() const -> QRectF {
        const QRectF imageBounds(QPointF(0.0, 0.0), QSizeF(m_imageSize));
        return QRectF(widgetToImage(QPointF(0.0, 0.0)),
                      widgetToImage(QPointF(static_cast<double>(m_viewportSize.width()),
                                            static_cast<double>(m_viewportSize.height()))))
            .normalized()
            .intersected(imageBounds);
    }

    /** @brief 获取完整图像适配视口所需的缩放倍率。 @return 适配缩放倍率。 */
    [[nodiscard]] auto fitScale() const -> double {
        if (m_imageSize.isEmpty() || m_viewportSize.isEmpty()) {
            return 1.0;
        }
        const auto scaleX =
            static_cast<double>(m_viewportSize.width()) / static_cast<double>(m_imageSize.width());
        const auto scaleY = static_cast<double>(m_viewportSize.height()) /
                            static_cast<double>(m_imageSize.height());
        return std::min(scaleX, scaleY);
    }

    /** @brief 获取当前图像尺寸。 @return 图像尺寸。 */
    [[nodiscard]] auto imageSize() const -> QSize {
        return m_imageSize;
    }

    /** @brief 获取当前缩放倍率。 @return 图像像素到控件像素的比例。 */
    [[nodiscard]] auto scaleFactor() const -> double {
        return m_scaleFactor;
    }

    /** @brief 获取图像左上角在控件坐标中的偏移。 @return 当前偏移。 */
    [[nodiscard]] auto offset() const -> QPointF {
        return m_offset;
    }

private:
    /** @brief 将缩放倍率和偏移约束在合法显示范围内。 */
    void clamp() {
        if (m_imageSize.isEmpty()) {
            return;
        }

        const auto minScale = fitScale();
        const auto maxScale = std::max(kMaxImageScaleFactor, minScale);
        m_scaleFactor = std::clamp(m_scaleFactor, minScale, maxScale);

        const auto scaledWidth = static_cast<double>(m_imageSize.width()) * m_scaleFactor;
        const auto scaledHeight = static_cast<double>(m_imageSize.height()) * m_scaleFactor;
        const auto viewportWidth = static_cast<double>(m_viewportSize.width());
        const auto viewportHeight = static_cast<double>(m_viewportSize.height());

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

    QSize m_imageSize;           ///< 当前图像尺寸。
    QSize m_viewportSize;        ///< 当前控件视口尺寸。
    double m_scaleFactor = 1.0;  ///< 当前缩放倍率。
    QPointF m_offset{};          ///< 图像左上角在控件坐标中的偏移。
};

}  // namespace Dss::Ui
