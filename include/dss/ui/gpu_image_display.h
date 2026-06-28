#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointF>
#include <QWheelEvent>
#include <cstdint>
#include <memory>
#include <vector>

namespace Dss::Ui {

/**
 * @brief 使用 OpenGL shader 显示图像并执行显示拉伸的图像控件。
 *
 * Manual 显示拉伸模式下，该控件直接接收 16-bit RAW 帧，将 low/high 阈值作为
 * shader uniform 更新，从而避免拖动 slider 时在 CPU 侧反复重建整张 8-bit 图像。
 * 当 RAW 帧不可用或 shader 初始化失败时，控件会回退为绘制普通 QImage。
 */
class GpuImageDisplay : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    /**
     * @brief 构造 GPU 图像显示控件。
     * @param parent Qt 父控件。
     */
    explicit GpuImageDisplay(QWidget* parent = nullptr);

    /**
     * @brief 析构控件并在 OpenGL 上下文有效时释放纹理资源。
     */
    ~GpuImageDisplay() override;

    /**
     * @brief 检查当前运行环境是否能创建可用的 OpenGL 上下文。
     * @return 可创建并绑定 OpenGL 上下文时返回 true。
     */
    [[nodiscard]] static bool isSupported();

    /**
     * @brief 设置 CPU fallback 显示图像。
     * @param image 待显示的灰度图像；会保存在控件内部。
     */
    void setImage(const QImage& image);

    /**
     * @brief 设置用于 GPU shader 显示拉伸的 16-bit RAW 帧。
     * @param rawImage RAW 像素缓冲，按行主序存储。
     * @param width 有效图像宽度，单位为像素。
     * @param height 有效图像高度，单位为像素。
     * @param stride 每行 RAW 像素跨度，单位为像素；允许大于 width。
     */
    void setRawFrame(std::shared_ptr<const std::vector<std::uint16_t>> rawImage,
                     std::uint32_t width, std::uint32_t height, std::uint32_t stride);

    /**
     * @brief 更新显示拉伸参数。
     * @param autoStretch 当前 UI 是否处于自动拉伸模式；GPU RAW 路径仅使用 manual low/high。
     * @param low 手动显示拉伸低阈值。
     * @param high 手动显示拉伸高阈值。
     */
    void setDisplayStretch(bool autoStretch, int low, int high);

    /**
     * @brief 将视图重置为完整图像适配当前控件区域。
     */
    void resetView();

    /**
     * @brief 将控件坐标转换为图像坐标。
     * @param widgetPos 控件局部坐标。
     * @return 对应的图像坐标。
     */
    [[nodiscard]] auto imagePositionAt(const QPointF& widgetPos) const -> QPointF;

    /**
     * @brief 获取当前图像显示缩放倍率。
     * @return 图像像素到控件像素的缩放倍率。
     */
    [[nodiscard]] auto imageScaleFactor() const -> double;

    /**
     * @brief 获取当前图像左上角在控件坐标中的偏移。
     * @return 当前显示偏移。
     */
    [[nodiscard]] auto imageOffset() const -> QPointF;

signals:
    /**
     * @brief 鼠标点击选中图像坐标时发出。
     * @param pos 被点击的图像坐标。
     */
    void positionClicked(QPointF pos);

    /**
     * @brief 鼠标在控件上移动时发出当前图像坐标。
     * @param pos 鼠标位置对应的图像坐标。
     */
    void mouseMoved(QPointF pos);

protected:
    /** @brief 初始化 OpenGL 函数、清屏颜色和 shader program。 */
    void initializeGL() override;

    /** @brief 绘制当前 RAW texture 或 fallback 图像。 */
    void paintGL() override;

    /**
     * @brief 同步 OpenGL viewport 尺寸。
     * @param width 新视口宽度。
     * @param height 新视口高度。
     */
    void resizeGL(int width, int height) override;

    /** @brief 处理左键选点和中键平移起点。 */
    void mousePressEvent(QMouseEvent* event) override;

    /** @brief 处理中键拖拽平移并发出鼠标图像坐标。 */
    void mouseMoveEvent(QMouseEvent* event) override;

    /** @brief 处理中键释放以结束平移状态。 */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /** @brief 控件尺寸变化时尽量保持原视图中心。 */
    void resizeEvent(QResizeEvent* event) override;

    /** @brief 处理滚轮缩放并保持鼠标锚点对应的图像位置不变。 */
    void wheelEvent(QWheelEvent* event) override;

private:
    /**
     * @brief 获取当前显示内容的图像尺寸。
     * @return RAW 帧尺寸或 fallback 图像尺寸。
     */
    [[nodiscard]] auto imageSize() const -> QSize;

    /**
     * @brief 计算完整图像适配控件区域所需的缩放倍率。
     * @return 适配缩放倍率。
     */
    [[nodiscard]] auto fitScale() const -> double;

    /**
     * @brief 将图像坐标转换为控件坐标。
     * @param imagePos 图像坐标。
     * @return 控件局部坐标。
     */
    [[nodiscard]] auto imageToWidget(const QPointF& imagePos) const -> QPointF;

    /** @brief 将当前偏移和缩放约束在可显示范围内。 */
    void clampOffset();

    /** @brief 在需要时上传或更新 RAW OpenGL texture。 */
    void uploadRawTextureIfNeeded();

    /** @brief 使用 QPainter 绘制 fallback QImage。 */
    void drawFallbackImage();

    /** @brief 释放当前 OpenGL texture。 */
    void releaseTexture();

    /**
     * @brief 检查当前 RAW 帧元数据与缓冲区大小是否匹配。
     * @return 当前 RAW 帧可安全上传时返回 true。
     */
    [[nodiscard]] auto hasRawFrame() const -> bool;

    std::shared_ptr<const std::vector<std::uint16_t>> m_rawImage;  ///< 当前 RAW 帧缓冲。
    std::vector<std::uint16_t> m_contiguousRawImage;  ///< stride 大于 width 时的连续 RAW 缓冲。
    QImage m_fallbackImage;                           ///< CPU fallback 显示图像。
    std::uint32_t m_rawWidth = 0;                     ///< 当前 RAW 帧有效宽度。
    std::uint32_t m_rawHeight = 0;                    ///< 当前 RAW 帧有效高度。
    std::uint32_t m_rawStride = 0;                    ///< 当前 RAW 帧行跨度。
    GLuint m_rawTexture = 0;                          ///< OpenGL RAW texture 句柄。
    bool m_rawTextureDirty = false;                   ///< RAW texture 是否需要重新上传。
    bool m_shaderReady = false;                       ///< shader program 是否成功初始化。
    QOpenGLShaderProgram m_program;                   ///< 执行显示拉伸的 shader program。
    int m_stretchLow = 1000;                          ///< 手动显示拉伸低阈值。
    int m_stretchHigh = 5000;                         ///< 手动显示拉伸高阈值。
    double m_scaleFactor = 1.0;                       ///< 当前显示缩放倍率。
    QPointF m_offset{};                               ///< 图像左上角在控件坐标中的偏移。
    bool m_isPanning = false;                         ///< 当前是否处于中键平移状态。
    QPointF m_lastPanPosition{};                      ///< 上一次平移鼠标位置。
};

}  // namespace Dss::Ui
