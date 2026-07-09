#include "dss/ui/display/gpu_image_display.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPainter>
#include <QResizeEvent>
#include <QSurfaceFormat>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Dss::Ui {
namespace {

constexpr auto kVertexShader = R"(
attribute vec2 position;
attribute vec2 texCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = texCoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

constexpr auto kFragmentShader = R"(
uniform sampler2D rawTexture;
uniform float low;
uniform float high;
varying vec2 vTexCoord;
void main() {
    float raw = texture2D(rawTexture, vTexCoord).r * 65535.0;
    float rangeValue = max(high - low, 1.0);
    float gray = clamp((raw - low) / rangeValue, 0.0, 1.0);
    gl_FragColor = vec4(gray, gray, gray, 1.0);
}
)";

[[nodiscard]] auto rawFramePayloadFits(
    const std::shared_ptr<const std::vector<std::uint16_t>>& rawImage, std::uint32_t width,
    std::uint32_t height, std::uint32_t stride) -> bool {
    if (!rawImage || width == 0 || height == 0 || stride < width) {
        return false;
    }
    const auto requiredSize =
        static_cast<std::size_t>(stride) * static_cast<std::size_t>(height - 1U) +
        static_cast<std::size_t>(width);
    return rawImage->size() >= requiredSize;
}

}  // namespace

GpuImageDisplay::GpuImageDisplay(QWidget* parent) : QOpenGLWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(320, 240);
}

GpuImageDisplay::~GpuImageDisplay() {
    if (isValid() && context() != nullptr && m_rawTexture != 0) {
        makeCurrent();
        releaseTexture();
        doneCurrent();
    }
}

bool GpuImageDisplay::isSupported() {
    QOpenGLContext context;
    if (!context.create()) {
        return false;
    }
    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid() || !context.makeCurrent(&surface)) {
        return false;
    }
    context.doneCurrent();
    return true;
}

void GpuImageDisplay::setImage(const QImage& image) {
    const auto keepViewport = m_viewport.imageSize() == image.size() && !image.isNull();
    m_rawImage.reset();
    m_contiguousRawImage.clear();
    m_rawWidth = 0;
    m_rawHeight = 0;
    m_rawStride = 0;
    m_rawTextureDirty = false;
    m_fallbackImage = image;
    m_viewport.setViewportSize(size());
    m_viewport.setImageSize(m_fallbackImage.size(), keepViewport);
    update();
}

void GpuImageDisplay::setRawFrame(std::shared_ptr<const std::vector<std::uint16_t>> rawImage,
                                  std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                                  int low, int high) {
    m_stretchLow = low;
    m_stretchHigh = std::max(high, low + 1);
    const QSize nextSize{static_cast<int>(width), static_cast<int>(height)};
    const auto keepViewport =
        m_viewport.imageSize() == nextSize && rawFramePayloadFits(rawImage, width, height, stride);
    m_fallbackImage = {};
    m_contiguousRawImage.clear();

    if (!rawFramePayloadFits(rawImage, width, height, stride)) {
        m_rawImage.reset();
        m_rawWidth = 0;
        m_rawHeight = 0;
        m_rawStride = 0;
        m_rawTextureDirty = false;
        m_viewport.clear();
        update();
        return;
    }

    m_rawImage = std::move(rawImage);
    m_rawWidth = width;
    m_rawHeight = height;
    m_rawStride = stride;
    if (m_rawStride != m_rawWidth) {
        m_contiguousRawImage.resize(static_cast<std::size_t>(m_rawWidth) *
                                    static_cast<std::size_t>(m_rawHeight));
        for (std::uint32_t row = 0; row < m_rawHeight; ++row) {
            const auto sourceOffset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(m_rawStride);
            const auto targetOffset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(m_rawWidth);
            std::copy_n(m_rawImage->data() + sourceOffset, m_rawWidth,
                        m_contiguousRawImage.data() + targetOffset);
        }
    }
    m_rawTextureDirty = true;
    m_viewport.setViewportSize(size());
    m_viewport.setImageSize(nextSize, keepViewport);
    update();
}

void GpuImageDisplay::setDisplayStretch(bool, int low, int high) {
    m_stretchLow = low;
    m_stretchHigh = std::max(high, low + 1);
    update();
}

void GpuImageDisplay::resetView() {
    m_viewport.setViewportSize(size());
    m_viewport.reset();
    update();
}

auto GpuImageDisplay::imagePositionAt(const QPointF& widgetPos) const -> QPointF {
    return m_viewport.widgetToImage(widgetPos);
}

auto GpuImageDisplay::imageScaleFactor() const -> double {
    return m_viewport.scaleFactor();
}

auto GpuImageDisplay::imageOffset() const -> QPointF {
    return m_viewport.offset();
}

void GpuImageDisplay::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    m_shaderReady = m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) &&
                    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader) &&
                    m_program.link();
}

void GpuImageDisplay::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (width() <= 0 || height() <= 0) {
        return;
    }
    if (!hasRawFrame() || !m_shaderReady) {
        drawFallbackImage();
        return;
    }

    uploadRawTextureIfNeeded();
    if (m_rawTexture == 0) {
        return;
    }

    const QRectF imageBounds(QPointF(0.0, 0.0), QSizeF(imageSize()));
    const auto sourceRect = m_viewport.visibleImageRect();
    if (sourceRect.isEmpty()) {
        return;
    }

    const QRectF targetRect(m_viewport.imageToWidget(sourceRect.topLeft()),
                            sourceRect.size() * m_viewport.scaleFactor());
    const auto left = static_cast<float>((targetRect.left() / width()) * 2.0 - 1.0);
    const auto right = static_cast<float>((targetRect.right() / width()) * 2.0 - 1.0);
    const auto top = static_cast<float>(1.0 - (targetRect.top() / height()) * 2.0);
    const auto bottom = static_cast<float>(1.0 - (targetRect.bottom() / height()) * 2.0);
    const auto u0 = static_cast<float>(sourceRect.left() / imageBounds.width());
    const auto u1 = static_cast<float>(sourceRect.right() / imageBounds.width());
    const auto v0 = static_cast<float>(sourceRect.top() / imageBounds.height());
    const auto v1 = static_cast<float>(sourceRect.bottom() / imageBounds.height());

    const std::array<float, 8> positions{left, bottom, right, bottom, left, top, right, top};
    const std::array<float, 8> texCoords{u0, v1, u1, v1, u0, v0, u1, v0};

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_rawTexture);
    m_program.bind();
    m_program.setUniformValue("rawTexture", 0);
    m_program.setUniformValue("low", static_cast<float>(m_stretchLow));
    m_program.setUniformValue("high", static_cast<float>(m_stretchHigh));
    m_program.enableAttributeArray("position");
    m_program.enableAttributeArray("texCoord");
    m_program.setAttributeArray("position", GL_FLOAT, positions.data(), 2);
    m_program.setAttributeArray("texCoord", GL_FLOAT, texCoords.data(), 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_program.disableAttributeArray("texCoord");
    m_program.disableAttributeArray("position");
    m_program.release();
}

void GpuImageDisplay::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);
}

void GpuImageDisplay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && !imageSize().isEmpty()) {
        m_isPanning = true;
        m_lastPanPosition = event->position();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        Q_EMIT positionClicked(imagePositionAt(event->position()));
    }
    QOpenGLWidget::mousePressEvent(event);
}

void GpuImageDisplay::mouseMoveEvent(QMouseEvent* event) {
    const auto isMiddleDrag = m_isPanning && event->buttons().testFlag(Qt::MiddleButton);
    if (isMiddleDrag) {
        const auto delta = event->position() - m_lastPanPosition;
        m_lastPanPosition = event->position();
        m_viewport.panBy(delta);
        update();
    }

    Q_EMIT mouseMoved(imagePositionAt(event->position()));
    if (isMiddleDrag) {
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GpuImageDisplay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void GpuImageDisplay::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    if (!imageSize().isEmpty()) {
        m_viewport.resizeKeepingCenter(event->oldSize(), event->size());
        update();
    }
}

void GpuImageDisplay::wheelEvent(QWheelEvent* event) {
    if (imageSize().isEmpty() || event->angleDelta().y() == 0) {
        QOpenGLWidget::wheelEvent(event);
        return;
    }

    m_viewport.setViewportSize(size());
    (void)m_viewport.zoomAt(event->position(), event->angleDelta().y());
    update();
    event->accept();
}

auto GpuImageDisplay::imageSize() const -> QSize {
    if (m_rawImage && m_rawWidth > 0 && m_rawHeight > 0) {
        return {static_cast<int>(m_rawWidth), static_cast<int>(m_rawHeight)};
    }
    return m_fallbackImage.size();
}

void GpuImageDisplay::uploadRawTextureIfNeeded() {
    if (!m_rawTextureDirty || !hasRawFrame()) {
        return;
    }
    if (m_rawTexture == 0) {
        glGenTextures(1, &m_rawTexture);
    }
    glBindTexture(GL_TEXTURE_2D, m_rawTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const auto* textureData =
        m_contiguousRawImage.empty() ? m_rawImage->data() : m_contiguousRawImage.data();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, static_cast<GLsizei>(m_rawWidth),
                 static_cast<GLsizei>(m_rawHeight), 0, GL_LUMINANCE, GL_UNSIGNED_SHORT,
                 textureData);
    m_rawTextureDirty = false;
}

void GpuImageDisplay::drawFallbackImage() {
    if (m_fallbackImage.isNull()) {
        return;
    }
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_viewport.scaleFactor() < 1.0);
    const auto sourceRect = m_viewport.visibleImageRect();
    if (!sourceRect.isEmpty()) {
        const QRectF targetRect(m_viewport.imageToWidget(sourceRect.topLeft()),
                                sourceRect.size() * m_viewport.scaleFactor());
        painter.drawImage(targetRect, m_fallbackImage, sourceRect);
    }
}

void GpuImageDisplay::releaseTexture() {
    if (m_rawTexture != 0) {
        glDeleteTextures(1, &m_rawTexture);
        m_rawTexture = 0;
    }
}

bool GpuImageDisplay::hasRawFrame() const {
    return rawFramePayloadFits(m_rawImage, m_rawWidth, m_rawHeight, m_rawStride);
}

}  // namespace Dss::Ui
