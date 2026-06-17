#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/processing/display_stretch.h"
#include "dss/processing/image_processor.h"
#include "dss/ui/display_view_model.h"
#include "dss/ui/view_model_context.h"

namespace {

class QCoreApplicationFixture {
public:
    QCoreApplicationFixture() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char appName[] = "test_display_view_model";
            static char* argv[] = {appName, nullptr};
            m_app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> m_app;
};

using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

[[nodiscard]] auto grayPixel(const QImage& image, int x, int y) -> std::uint8_t {
    return static_cast<std::uint8_t>(image.constScanLine(y)[x]);
}

}  // namespace

TEST(DisplayViewModel, AppliesDisplayStretchSettingsToImageProcessor) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});

    ASSERT_TRUE(display.applyDisplayStretch(false, 1000, 5000));
    auto settings = processor->displayStretchSettings();
    EXPECT_EQ(settings.mode, Dss::Processing::DisplayStretchMode::Manual);
    EXPECT_EQ(settings.low, 1000U);
    EXPECT_EQ(settings.high, 5000U);

    EXPECT_FALSE(display.applyDisplayStretch(false, 5000, 1000));
    settings = processor->displayStretchSettings();
    EXPECT_EQ(settings.low, 1000U);
    EXPECT_EQ(settings.high, 5000U);
}

TEST(DisplayViewModel, RebuildsCurrentDisplayWhenStretchSettingsChange) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::DisplayViewModel displayViewModel({.bus = bus, .registry = registry});

    std::vector<QImage> images;
    auto connection =
        QObject::connect(&displayViewModel, &Dss::Ui::DisplayViewModel::displayImageReady,
                         [&images](const QImage& image) { images.push_back(image.copy()); });

    auto display =
        std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9, 9, 9, 9});
    auto raw = std::make_shared<const std::vector<std::uint16_t>>(
        std::vector<std::uint16_t>{500, 1000, 3000, 5000});
    bus.emit(Dss::Core::DisplayRefreshEvent{0, 2, 2, 2, std::move(display), raw});
    ASSERT_EQ(images.size(), 1U);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(false, 1000, 5000));
    ASSERT_EQ(images.size(), 2U);
    ASSERT_EQ(images.back().size(), QSize(2, 2));
    EXPECT_EQ(grayPixel(images.back(), 0, 0), 0U);
    EXPECT_EQ(grayPixel(images.back(), 1, 0), 0U);
    EXPECT_EQ(grayPixel(images.back(), 0, 1), 127U);
    EXPECT_EQ(grayPixel(images.back(), 1, 1), 255U);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(true, 1000, 5000));
    ASSERT_EQ(images.size(), 3U);
    const auto expectedAuto =
        Dss::Processing::buildDisplayImage(*raw, Dss::Processing::DisplayStretchSettings{})
            .displayImage;
    ASSERT_EQ(expectedAuto.size(), 4U);
    EXPECT_EQ(grayPixel(images.back(), 0, 0), expectedAuto[0]);
    EXPECT_EQ(grayPixel(images.back(), 1, 0), expectedAuto[1]);
    EXPECT_EQ(grayPixel(images.back(), 0, 1), expectedAuto[2]);
    EXPECT_EQ(grayPixel(images.back(), 1, 1), expectedAuto[3]);

    QObject::disconnect(connection);
}
