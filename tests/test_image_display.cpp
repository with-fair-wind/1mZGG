#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPoint>
#include <QWheelEvent>
#include <memory>

#include <gtest/gtest.h>

#include "dss/ui/image_display.h"

namespace {

auto ensureApplication() -> QApplication& {
    static int argc = 1;
    static char appName[] = "test_image_display";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QApplication> app;

    if (QApplication::instance() == nullptr) {
        app = std::make_unique<QApplication>(argc, argv);
    }
    return *qobject_cast<QApplication*>(QApplication::instance());
}

void sendWheel(Dss::Ui::ImageDisplay& display, const QPointF& position, int angleDeltaY) {
    QWheelEvent event(position, position, QPoint(), QPoint(0, angleDeltaY), Qt::NoButton,
                      Qt::NoModifier, Qt::ScrollUpdate, false);
    QApplication::sendEvent(&display, &event);
}

void sendMouseEvent(Dss::Ui::ImageDisplay& display, QEvent::Type type, const QPointF& position,
                    Qt::MouseButton button, Qt::MouseButtons buttons) {
    const QPointF globalPosition{display.mapToGlobal(position.toPoint())};
    QMouseEvent event(type, position, position, globalPosition, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&display, &event);
}

void sendMiddleDrag(Dss::Ui::ImageDisplay& display, const QPointF& start, const QPointF& end) {
    sendMouseEvent(display, QEvent::MouseButtonPress, start, Qt::MiddleButton, Qt::MiddleButton);
    sendMouseEvent(display, QEvent::MouseMove, end, Qt::NoButton, Qt::MiddleButton);
    sendMouseEvent(display, QEvent::MouseButtonRelease, end, Qt::MiddleButton, Qt::NoButton);
}

}  // namespace

TEST(ImageDisplay, WheelZoomKeepsCursorAnchoredToImagePixel) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));

    const QPointF cursor{512.0, 512.0};
    const auto before = display.imagePositionAt(cursor);
    const auto initialScale = display.imageScaleFactor();

    sendWheel(display, cursor, 120);

    const auto after = display.imagePositionAt(cursor);
    EXPECT_GT(display.imageScaleFactor(), initialScale);
    EXPECT_NEAR(after.x(), before.x(), 1.0e-6);
    EXPECT_NEAR(after.y(), before.y(), 1.0e-6);
}

TEST(ImageDisplay, WheelZoomOutStopsAtWholeImageFit) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));

    const auto fitScale = display.imageScaleFactor();

    sendWheel(display, QPointF{512.0, 512.0}, -120);

    EXPECT_DOUBLE_EQ(display.imageScaleFactor(), fitScale);
}

TEST(ImageDisplay, KeepsViewportWhenImageSequenceUsesSameSize) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));

    const QPointF cursor{768.0, 384.0};
    sendWheel(display, cursor, 120);
    const auto scaleAfterZoom = display.imageScaleFactor();
    const auto imagePosAfterZoom = display.imagePositionAt(cursor);

    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));

    const auto imagePosAfterNextFrame = display.imagePositionAt(cursor);
    EXPECT_DOUBLE_EQ(display.imageScaleFactor(), scaleAfterZoom);
    EXPECT_NEAR(imagePosAfterNextFrame.x(), imagePosAfterZoom.x(), 1.0e-6);
    EXPECT_NEAR(imagePosAfterNextFrame.y(), imagePosAfterZoom.y(), 1.0e-6);
}

TEST(ImageDisplay, MiddleButtonDragMovesZoomedImageByPointerDelta) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));
    sendWheel(display, QPointF{512.0, 512.0}, 120);
    const auto offsetBefore = display.imageOffset();

    sendMiddleDrag(display, QPointF{512.0, 512.0}, QPointF{562.0, 542.0});

    EXPECT_NEAR(display.imageOffset().x(), offsetBefore.x() + 50.0, 1.0e-6);
    EXPECT_NEAR(display.imageOffset().y(), offsetBefore.y() + 30.0, 1.0e-6);
}

TEST(ImageDisplay, MiddleButtonDragClampsZoomedImageAtViewportEdges) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));
    sendWheel(display, QPointF{512.0, 512.0}, 120);

    sendMiddleDrag(display, QPointF{512.0, 512.0}, QPointF{5000.0, 5000.0});
    EXPECT_NEAR(display.imageOffset().x(), 0.0, 1.0e-6);
    EXPECT_NEAR(display.imageOffset().y(), 0.0, 1.0e-6);

    sendMiddleDrag(display, QPointF{512.0, 512.0}, QPointF{-5000.0, -5000.0});
    const auto minimumOffset = 1024.0 - 6144.0 * display.imageScaleFactor();
    EXPECT_NEAR(display.imageOffset().x(), minimumOffset, 1.0e-6);
    EXPECT_NEAR(display.imageOffset().y(), minimumOffset, 1.0e-6);
}

TEST(ImageDisplay, MiddleButtonDragKeepsFittedImageCentered) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));
    const auto offsetBefore = display.imageOffset();

    sendMiddleDrag(display, QPointF{512.0, 512.0}, QPointF{800.0, 700.0});

    EXPECT_EQ(display.imageOffset(), offsetBefore);
}

TEST(ImageDisplay, MiddleButtonReleaseStopsPanningUntilNextPress) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ImageDisplay display;
    display.resize(1024, 1024);
    display.setImage(QImage(6144, 6144, QImage::Format_Grayscale8));
    sendWheel(display, QPointF{512.0, 512.0}, 120);

    sendMouseEvent(display, QEvent::MouseButtonPress, QPointF{512.0, 512.0}, Qt::MiddleButton,
                   Qt::MiddleButton);
    sendMouseEvent(display, QEvent::MouseMove, QPointF{532.0, 522.0}, Qt::NoButton,
                   Qt::MiddleButton);
    sendMouseEvent(display, QEvent::MouseButtonRelease, QPointF{532.0, 522.0}, Qt::MiddleButton,
                   Qt::NoButton);
    const auto offsetAfterRelease = display.imageOffset();

    sendMouseEvent(display, QEvent::MouseMove, QPointF{552.0, 542.0}, Qt::NoButton,
                   Qt::MiddleButton);

    EXPECT_EQ(display.imageOffset(), offsetAfterRelease);
}
