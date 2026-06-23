#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QPointF>
#include <QWheelEvent>
#include <QWidget>

namespace Dss::Ui {

/// 可缩放、平移的灰度图像显示控件
class ImageDisplay : public QWidget {
    Q_OBJECT

public:
    explicit ImageDisplay(QWidget* parent = nullptr);

    /// 设置待显示图像
    void setImage(const QImage& image);
    /// 重置视口为适应窗口大小
    void resetView();

    /**
     * @brief 将控件坐标转换为图像坐标
     * @param widgetPos 控件内坐标
     * @return 对应的图像坐标
     */
    [[nodiscard]] auto imagePositionAt(const QPointF& widgetPos) const -> QPointF;
    /// 当前缩放比例
    [[nodiscard]] auto imageScaleFactor() const -> double;
    /// 当前图像偏移量（控件坐标系）
    [[nodiscard]] auto imageOffset() const -> QPointF;

signals:
    /// 鼠标左键点击位置（图像坐标）
    void positionClicked(QPointF pos);
    /// 鼠标移动位置（图像坐标）
    void mouseMoved(QPointF pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// 计算适应窗口的最小缩放比例
    [[nodiscard]] auto fitScale() const -> double;
    /// 将图像坐标转换为控件坐标
    [[nodiscard]] auto imageToWidget(const QPointF& imagePos) const -> QPointF;

    /// 限制偏移量与缩放比例在有效范围内
    void clampOffset();

    QImage m_currentImage;        ///< 当前显示图像
    double m_scaleFactor = 1.0;   ///< 当前缩放比例
    QPointF m_offset{};           ///< 图像左上角在控件中的偏移
    bool m_isPanning = false;     ///< 是否正在使用中键拖动图像
    QPointF m_lastPanPosition{};  ///< 上一次拖动位置（控件坐标）
};

}  // namespace Dss::Ui
