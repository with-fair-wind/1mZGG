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
    /**
     * @brief 创建可缩放和平移的图像显示控件。
     * @param parent Qt 父控件。
     */
    explicit ImageDisplay(QWidget* parent = nullptr);

    /**
     * @brief 设置待显示图像并重置视口。
     * @param image Qt 图像；控件保留隐式共享副本。
     */
    void setImage(const QImage& image);
    /// 重置视口为适应窗口大小
    void resetView();

    /**
     * @brief 将控件坐标转换为图像坐标
     * @param widgetPos 控件内坐标
     * @return 对应的图像坐标
     */
    [[nodiscard]] auto imagePositionAt(const QPointF& widgetPos) const -> QPointF;
    /** @brief 获取当前缩放比例。 @return 图像像素到控件像素的比例。 */
    [[nodiscard]] auto imageScaleFactor() const -> double;
    /**
     * @brief 获取当前图像偏移量。
     * @return 图像左上角在控件坐标系中的位置。
     */
    [[nodiscard]] auto imageOffset() const -> QPointF;

signals:
    /**
     * @brief 鼠标左键点击位置。
     * @param pos 图像坐标系中的位置。
     */
    void positionClicked(QPointF pos);
    /**
     * @brief 鼠标移动位置。
     * @param pos 图像坐标系中的位置。
     */
    void mouseMoved(QPointF pos);

protected:
    /**
     * @brief 绘制当前视口内的图像区域。
     * @param event 绘制事件。
     */
    void paintEvent(QPaintEvent* event) override;

    /** @brief 开始目标选择或中键平移。 @param event 鼠标按下事件。 */
    void mousePressEvent(QMouseEvent* event) override;
    /** @brief 更新鼠标位置或中键平移偏移。 @param event 鼠标移动事件。 */
    void mouseMoveEvent(QMouseEvent* event) override;
    /** @brief 结束中键平移。 @param event 鼠标释放事件。 */
    void mouseReleaseEvent(QMouseEvent* event) override;
    /** @brief 在控件尺寸变化后约束视口。 @param event 尺寸变化事件。 */
    void resizeEvent(QResizeEvent* event) override;
    /** @brief 以鼠标位置为锚点缩放图像。 @param event 滚轮事件。 */
    void wheelEvent(QWheelEvent* event) override;

private:
    /**
     * @brief 计算整幅图像适应控件的缩放比例。
     * @return 保持纵横比并完整显示图像的最小比例。
     */
    [[nodiscard]] auto fitScale() const -> double;
    /**
     * @brief 将图像坐标转换为控件坐标。
     * @param imagePos 图像坐标。
     * @return 应用当前缩放和偏移后的控件坐标。
     */
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
