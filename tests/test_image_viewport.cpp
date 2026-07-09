#include <QPointF>
#include <QSize>

#include <gtest/gtest.h>

#include "dss/ui/image_viewport.h"

TEST(ImageViewport, ResetFitsImageAndConvertsCoordinates) {
    Dss::Ui::ImageViewport viewport;
    viewport.setViewportSize(QSize{1024, 1024});
    viewport.setImageSize(QSize{6144, 6144}, false);

    EXPECT_DOUBLE_EQ(viewport.scaleFactor(), 1024.0 / 6144.0);
    EXPECT_DOUBLE_EQ(viewport.offset().x(), 0.0);
    EXPECT_DOUBLE_EQ(viewport.offset().y(), 0.0);

    const auto imagePos = viewport.widgetToImage(QPointF{512.0, 256.0});
    EXPECT_NEAR(imagePos.x(), 3072.0, 1.0e-6);
    EXPECT_NEAR(imagePos.y(), 1536.0, 1.0e-6);
}

TEST(ImageViewport, ZoomKeepsAnchorImagePositionStable) {
    Dss::Ui::ImageViewport viewport;
    viewport.setViewportSize(QSize{1024, 1024});
    viewport.setImageSize(QSize{6144, 6144}, false);

    const QPointF anchor{768.0, 384.0};
    const auto before = viewport.widgetToImage(anchor);
    ASSERT_TRUE(viewport.zoomAt(anchor, 120));
    const auto after = viewport.widgetToImage(anchor);

    EXPECT_GT(viewport.scaleFactor(), viewport.fitScale());
    EXPECT_NEAR(after.x(), before.x(), 1.0e-6);
    EXPECT_NEAR(after.y(), before.y(), 1.0e-6);
}

TEST(ImageViewport, PanClampsAtViewportEdges) {
    Dss::Ui::ImageViewport viewport;
    viewport.setViewportSize(QSize{1024, 1024});
    viewport.setImageSize(QSize{6144, 6144}, false);
    ASSERT_TRUE(viewport.zoomAt(QPointF{512.0, 512.0}, 120));

    viewport.panBy(QPointF{5000.0, 5000.0});
    EXPECT_NEAR(viewport.offset().x(), 0.0, 1.0e-6);
    EXPECT_NEAR(viewport.offset().y(), 0.0, 1.0e-6);

    viewport.panBy(QPointF{-10000.0, -10000.0});
    const auto minimumOffset = 1024.0 - 6144.0 * viewport.scaleFactor();
    EXPECT_NEAR(viewport.offset().x(), minimumOffset, 1.0e-6);
    EXPECT_NEAR(viewport.offset().y(), minimumOffset, 1.0e-6);
}

TEST(ImageViewport, ResizeKeepsPreviousCenterImagePosition) {
    Dss::Ui::ImageViewport viewport;
    viewport.setViewportSize(QSize{1000, 500});
    viewport.setImageSize(QSize{4000, 2000}, false);
    ASSERT_TRUE(viewport.zoomAt(QPointF{500.0, 250.0}, 120));

    const auto before = viewport.widgetToImage(QPointF{500.0, 250.0});
    viewport.resizeKeepingCenter(QSize{1000, 500}, QSize{1200, 800});
    const auto after = viewport.widgetToImage(QPointF{600.0, 400.0});

    EXPECT_NEAR(after.x(), before.x(), 1.0e-6);
    EXPECT_NEAR(after.y(), before.y(), 1.0e-6);
}
