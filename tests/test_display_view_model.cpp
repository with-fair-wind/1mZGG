#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/core/event/message_bus.h"
#include "dss/core/service/service_registry.h"
#include "dss/processing/frame/display_stretch.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/ui/view_model/display_view_model.h"
#include "dss/ui/view_model/view_model_context.h"

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

using MessageBus = Dss::Core::MessageBus;

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

TEST(DisplayViewModel, SlowUiRetainsOnlyLatestPendingFrame) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});
    int count = 0;
    int lastPixel = -1;
    QObject::connect(&display, &Dss::Ui::DisplayViewModel::displayImageReady, &display,
                     [&](const QImage& image) {
                         ++count;
                         lastPixel = grayPixel(image, 0, 0);
                     });
    std::vector<std::weak_ptr<const std::vector<std::uint16_t>>> buffers;
    std::jthread producer([&] {
        for (int index = 0; index < 100; ++index) {
            auto raw = std::make_shared<const std::vector<std::uint16_t>>(1, index);
            buffers.push_back(raw);
            auto image = std::make_shared<const std::vector<std::uint8_t>>(1, index);
            bus.emit(Dss::Core::DisplayRefreshEvent{static_cast<std::uint64_t>(index), 1, 1, 1,
                                                    image, raw});
        }
    });
    producer.join();  // UI 未处理事件，生产线程已经投递 100 帧。
    int alive = 0;
    for (const auto& buffer : buffers) {
        alive += !buffer.expired();
    }
    EXPECT_EQ(alive, 1);
    EXPECT_EQ(count, 0);
    QCoreApplication::processEvents();
    EXPECT_EQ(count, 1);
    EXPECT_EQ(lastPixel, 99);
}

TEST(DisplayViewModel, SessionResetDiscardsQueuedOldFrame) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});
    int count = 0;
    QObject::connect(&display, &Dss::Ui::DisplayViewModel::displayImageReady, &display,
                     [&](const QImage&) { ++count; });
    std::jthread producer([&] {
        auto image = std::make_shared<const std::vector<std::uint8_t>>(1, 42);
        bus.emit(Dss::Core::DisplayRefreshEvent{1, 1, 1, 1, image, nullptr});
    });
    producer.join();
    bus.emit(Dss::Core::ProcessingSessionResetEvent{});
    QCoreApplication::processEvents();
    EXPECT_EQ(count, 0);
}

TEST(DisplayViewModel, ResetDuringFrameDeliveryDiscardsOldStatsAndAcceptsNewFrame) {
    for (const bool rawMode : {false, true}) {
        QCoreApplicationFixture app;
        MessageBus bus;
        Dss::Core::ServiceRegistry registry;
        Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});
        display.setRawDisplayEnabled(rawMode);
        int deliveredFrames = 0;
        std::vector<double> deliveredStats;
        const auto publishFrame = [&](std::uint64_t sequence, std::uint16_t value) {
            Dss::Core::DisplayRefreshEvent event{};
            event.frameSeq = sequence;
            event.width = event.height = event.stride = 1;
            event.rawImage = std::make_shared<const std::vector<std::uint16_t>>(1, value);
            event.displayImage = std::make_shared<const std::vector<std::uint8_t>>(1, value);
            event.displayStretchWindowValid = true;
            event.displayStretchHigh = 255;
            bus.emit(event);
            Dss::Core::ImageStats stats{};
            stats.avg = value;
            bus.emit(Dss::Core::ProcessingCompleteEvent{sequence, stats});
        };
        const auto onFrame = [&] {
            if (++deliveredFrames == 1) {
                // Like ReplaySession, reset on a worker while the UI batch is in flight.
                std::jthread resetter([&] {
                    bus.emit(Dss::Core::ProcessingSessionResetEvent{});
                    publishFrame(0, 99);
                });
                resetter.join();
                EXPECT_EQ(display.resourceSnapshot().activeItems, 0U);
            }
        };
        QObject::connect(&display, &Dss::Ui::DisplayViewModel::displayImageReady, &display,
                         [&](const QImage&) { onFrame(); });
        QObject::connect(&display, &Dss::Ui::DisplayViewModel::rawDisplayFrameReady, &display,
                         [&](auto, auto, auto, auto, auto, auto) { onFrame(); });
        QObject::connect(&display, &Dss::Ui::DisplayViewModel::imageStatsUpdated, &display,
                         [&](auto, auto, double avg, auto) { deliveredStats.push_back(avg); });
        std::jthread producer([&] { publishFrame(12, 42); });
        producer.join();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        EXPECT_EQ(deliveredFrames, 2);
        EXPECT_EQ(deliveredStats, std::vector<double>{99});
        EXPECT_EQ(display.resourceSnapshot().activeItems, 1U);
        display.clearCurrentDisplayFrame();
        EXPECT_EQ(display.resourceSnapshot().activeItems, 0U);
    }
}

TEST(DisplayViewModel, ResetDuringStretchStatsPreventsOldImageRedraw) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});
    ASSERT_TRUE(display.applyDisplayStretch(false, 1000, 5000));
    auto raw = std::make_shared<const std::vector<std::uint16_t>>(4, 2000);
    bus.emit(Dss::Core::DisplayRefreshEvent{1, 2, 2, 2, nullptr, raw});
    int redraws = 0;
    int stats = 0;
    QObject::connect(&display, &Dss::Ui::DisplayViewModel::displayImageReady, &display,
                     [&](const QImage&) { ++redraws; });
    QObject::connect(&display, &Dss::Ui::DisplayViewModel::imageStatsUpdated, &display,
                     [&](auto, auto, auto, auto) {
                         ++stats;
                         bus.emit(Dss::Core::ProcessingSessionResetEvent{});
                     });
    ASSERT_TRUE(display.applyDisplayStretch(true, 1000, 5000));
    EXPECT_EQ(stats, 1);
    EXPECT_EQ(redraws, 0);
    EXPECT_EQ(display.resourceSnapshot().activeItems, 0U);
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

TEST(DisplayViewModel, RawDisplayModeUsesRawFrameWhileAutoStretchIsEnabled) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::DisplayViewModel displayViewModel({.bus = bus, .registry = registry});
    displayViewModel.setRawDisplayEnabled(true);

    int imageReadyCount = 0;
    int rawReadyCount = 0;
    int lastLow = 0;
    int lastHigh = 0;
    const auto imageConnection =
        QObject::connect(&displayViewModel, &Dss::Ui::DisplayViewModel::displayImageReady,
                         [&imageReadyCount](const QImage&) { ++imageReadyCount; });
    const auto rawConnection =
        QObject::connect(&displayViewModel, &Dss::Ui::DisplayViewModel::rawDisplayFrameReady,
                         [&rawReadyCount, &lastLow, &lastHigh](
                             const std::shared_ptr<const std::vector<std::uint16_t>>&,
                             std::uint32_t, std::uint32_t, std::uint32_t, int low, int high) {
                             ++rawReadyCount;
                             lastLow = low;
                             lastHigh = high;
                         });

    auto display =
        std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9, 9, 9, 9});
    auto raw = std::make_shared<const std::vector<std::uint16_t>>(
        std::vector<std::uint16_t>{500, 1000, 3000, 5000});
    Dss::Core::DisplayRefreshEvent event{};
    event.frameSeq = 7;
    event.width = 2;
    event.height = 2;
    event.stride = 2;
    event.displayImage = std::move(display);
    event.rawImage = raw;
    event.displayStretchLow = 120;
    event.displayStretchHigh = 3400;
    event.displayStretchWindowValid = true;
    bus.emit(event);

    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 1);
    EXPECT_EQ(lastLow, 120);
    EXPECT_EQ(lastHigh, 3400);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(false, 1000, 5000));
    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 2);
    EXPECT_EQ(lastLow, 1000);
    EXPECT_EQ(lastHigh, 5000);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(true, 1000, 5000));
    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 3);
    EXPECT_EQ(lastLow, 120);
    EXPECT_EQ(lastHigh, 3400);

    QObject::disconnect(imageConnection);
    QObject::disconnect(rawConnection);
}

TEST(DisplayViewModel, RawDisplayModeEmitsRawFrameAndSkipsCpuRebuildOnStretchChange) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::DisplayViewModel displayViewModel({.bus = bus, .registry = registry});
    displayViewModel.setRawDisplayEnabled(true);
    ASSERT_TRUE(displayViewModel.applyDisplayStretch(false, 1000, 5000));

    int imageReadyCount = 0;
    int rawReadyCount = 0;
    std::uint32_t lastRawWidth = 0;
    const auto imageConnection =
        QObject::connect(&displayViewModel, &Dss::Ui::DisplayViewModel::displayImageReady,
                         [&imageReadyCount](const QImage&) { ++imageReadyCount; });
    const auto rawConnection =
        QObject::connect(&displayViewModel, &Dss::Ui::DisplayViewModel::rawDisplayFrameReady,
                         [&rawReadyCount, &lastRawWidth](
                             const std::shared_ptr<const std::vector<std::uint16_t>>&,
                             std::uint32_t width, std::uint32_t, std::uint32_t, int, int) {
                             ++rawReadyCount;
                             lastRawWidth = width;
                         });

    auto display =
        std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9, 9, 9, 9});
    auto raw = std::make_shared<const std::vector<std::uint16_t>>(
        std::vector<std::uint16_t>{500, 1000, 3000, 5000});
    bus.emit(Dss::Core::DisplayRefreshEvent{7, 2, 2, 2, std::move(display), raw});

    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 1);
    EXPECT_EQ(lastRawWidth, 2U);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(false, 1000, 5000));
    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 1);

    ASSERT_TRUE(displayViewModel.applyDisplayStretch(false, 1200, 5000));
    EXPECT_EQ(imageReadyCount, 0);
    EXPECT_EQ(rawReadyCount, 1);

    QObject::disconnect(imageConnection);
    QObject::disconnect(rawConnection);
}

TEST(DisplayViewModel, ResourcesCountRetainedCapacityAndClearCachedRaw) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DisplayViewModel display({.bus = bus, .registry = registry});
    std::jthread producer([&] {
        for (int index = 0; index < 3; ++index) {
            std::vector<std::uint16_t> raw(1, 1);
            raw.reserve(16);
            Dss::Core::DisplayRefreshEvent event{};
            event.width = 1;
            event.height = 1;
            event.stride = 1;
            event.rawImage = std::make_shared<const std::vector<std::uint16_t>>(std::move(raw));
            bus.emit(event);
        }
    });
    producer.join();
    auto snapshot = display.resourceSnapshot();
    EXPECT_EQ(snapshot.queuedItems, 1U);
    EXPECT_EQ(snapshot.queuedBytes, 32U);
    EXPECT_EQ(snapshot.replacedItems, 2U);
    QCoreApplication::processEvents();
    snapshot = display.resourceSnapshot();
    EXPECT_EQ(snapshot.queuedBytes, 0U);
    EXPECT_EQ(snapshot.activeBytes, 32U);
    EXPECT_EQ(snapshot.completedItems, 1U);
    display.clearCurrentDisplayFrame();
    snapshot = display.resourceSnapshot();
    EXPECT_EQ(snapshot.activeBytes, 0U);
    EXPECT_EQ(snapshot.peakQueuedBytes, 32U);
}
