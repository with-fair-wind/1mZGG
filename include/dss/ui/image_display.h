#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QPointF>
#include <QWheelEvent>
#include <QWidget>

namespace Dss::Ui {

class ImageDisplay : public QWidget {
    Q_OBJECT

public:
    explicit ImageDisplay(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void resetView();

    [[nodiscard]] auto imagePositionAt(const QPointF& widgetPos) const -> QPointF;
    [[nodiscard]] auto imageScaleFactor() const -> double;
    [[nodiscard]] auto imageOffset() const -> QPointF;

signals:
    void positionClicked(QPointF pos);
    void mouseMoved(QPointF pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    [[nodiscard]] auto fitScale() const -> double;
    [[nodiscard]] auto imageToWidget(const QPointF& imagePos) const -> QPointF;

    void clampOffset();

    QImage m_currentImage;
    double m_scaleFactor = 1.0;
    QPointF m_offset{};
};

}  // namespace Dss::Ui
