#pragma once

#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>

namespace Dss::Ui {

class ImageDisplay : public QWidget {
    Q_OBJECT

public:
    explicit ImageDisplay(QWidget* parent = nullptr);

    void setImage(const QImage& image);

signals:
    void positionClicked(QPointF pos);
    void mouseMoved(QPointF pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    [[nodiscard]] QPointF widgetToImage(const QPointF& widgetPos) const;

    QImage m_currentImage;
    QImage m_scaledImage;
    double m_scaleFactor = 1.0;
    QPointF m_offset{};
};

}  // namespace Dss::Ui
