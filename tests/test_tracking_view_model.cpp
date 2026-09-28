#include <QCoreApplication>
#include <QEventLoop>
#include <QPointF>
#include <QThread>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/ui/view_model/tracking_view_model.h"
#include "dss/ui/view_model/view_model_context.h"

using namespace std::chrono_literals;

namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_tracking_view_model";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

template <typename T>
auto waitForReady(QCoreApplication& app, std::future<T>& future,
                  std::chrono::milliseconds timeout = 2s) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (future.wait_for(0ms) == std::future_status::ready) {
            return true;
        }
        app.processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(5ms);
    }
    return future.wait_for(0ms) == std::future_status::ready;
}

}  // namespace

TEST(TrackingViewModel, SelectTargetEnablesManualTrackingOnImageProcessor) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::TrackingViewModel tracking(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    std::promise<Dss::Core::TrackResultEvent> trackPromise;
    auto trackFuture = trackPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::TrackResultEvent>(
        [&trackPromise](Dss::Core::TrackResultEvent event) {
            trackPromise.set_value(std::move(event));
        });

    tracking.selectTarget(QPointF{120.0, 80.0});

    EXPECT_EQ(tracking.trackMode(), static_cast<int>(Dss::Core::TrackMode::Manual));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Manual);

    Dss::Processing::FramePacket packet{};
    packet.frameSeq = 12;
    packet.width = 4;
    packet.height = 4;
    packet.displayImage = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    packet.metadata.pointingAe = Dss::Core::Vec2f{1.0F, 2.0F};
    packet.metadata.frameFrequency = 10.0F;

    processor->start();
    ASSERT_TRUE(processor->submitFrame(std::move(packet)));

    ASSERT_EQ(trackFuture.wait_for(2s), std::future_status::ready);
    processor->stop();

    const auto event = trackFuture.get();
    ASSERT_EQ(event.targets.size(), 1U);
    EXPECT_EQ(event.frameSeq, 12U);
    EXPECT_EQ(event.targets.front().targetId, "manual");
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.x, 120.0F);
    EXPECT_FLOAT_EQ(event.targets.front().predictedPosFrame.y, 80.0F);
}

TEST(TrackingViewModel, SetTrackModeConfiguresNonManualTrackingStrategies) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::TrackingViewModel tracking(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    tracking.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Geo));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Geo);

    tracking.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Leo));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Leo);

    tracking.setTrackMode(static_cast<int>(Dss::Core::TrackMode::SpaceCatalog));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::SpaceCatalog);

    tracking.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Init));
    EXPECT_EQ(processor->currentTrackMode(), Dss::Core::TrackMode::Init);
}

TEST(TrackingViewModel, RepeatingSameModePreservesTrackingHistory) {
    (void)ensureApplication();
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::TrackingViewModel tracking({.bus = bus, .registry = registry});
    std::vector<std::size_t> historyLengths;
    auto connection = bus.subscribe<Dss::Core::TrackResultEvent>([&](const auto& event) {
        if (!event.targets.empty()) {
            historyLengths.push_back(event.targets.front().frameInfos.size());
        }
    });
    tracking.selectTarget(QPointF{1, 1});
    for (std::uint64_t index = 0; index < 2; ++index) {
        tracking.setTrackMode(static_cast<int>(Dss::Core::TrackMode::Manual));
        Dss::Processing::FramePacket packet{};
        packet.frameSeq = index;
        packet.width = 2;
        packet.height = 2;
        packet.displayImage = {1, 2, 3, 4};
        processor->start();
        ASSERT_TRUE(processor->submitFrame(std::move(packet)));
        processor->drain();
    }
    EXPECT_EQ(historyLengths, (std::vector<std::size_t>{1, 2}));
}

TEST(TrackingViewModel, ResetDiscardsQueuedResultsFromPreviousSession) {
    auto& app = ensureApplication();
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::TrackingViewModel tracking({.bus = bus, .registry = registry});
    std::vector<int> counts;
    QObject::connect(&tracking, &Dss::Ui::TrackingViewModel::targetListUpdated, &tracking,
                     [&](int count) { counts.push_back(count); });
    std::jthread worker([&] {
        Dss::Core::TrackResultEvent event;
        event.targets.resize(1);
        bus.emit(event);
    });
    worker.join();
    bus.emit(Dss::Core::ProcessingSessionResetEvent{});
    app.processEvents();
    EXPECT_EQ(counts, (std::vector<int>{0}));
}

TEST(TrackingViewModel, TrackResultFromWorkerThreadUpdatesOnObjectThread) {
    auto& app = ensureApplication();

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::TrackingViewModel tracking(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    std::promise<QThread*> signalThreadPromise;
    auto signalThreadFuture = signalThreadPromise.get_future();
    QObject::connect(
        &tracking, &Dss::Ui::TrackingViewModel::targetListUpdated,
        [&signalThreadPromise](int) { signalThreadPromise.set_value(QThread::currentThread()); });

    Dss::Core::TargetInfo target{};
    target.targetId = "worker";
    std::jthread worker([&bus, target] {
        bus.emit(Dss::Core::TrackResultEvent{.frameSeq = 3, .targets = {target}});
    });
    worker.join();

    ASSERT_TRUE(waitForReady(app, signalThreadFuture));
    EXPECT_EQ(signalThreadFuture.get(), tracking.thread());
}

TEST(TrackingViewModel, CoalescesWorkerResultsWhileUiIsBusy) {
    auto& app = ensureApplication();
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::TrackingViewModel tracking({.bus = bus, .registry = registry});
    std::vector<QString> updates;
    QObject::connect(&tracking, &Dss::Ui::TrackingViewModel::trackInfoUpdated, &tracking,
                     [&](const QString& info) { updates.push_back(info); });
    std::jthread worker([&] {
        for (int index = 0; index < 100; ++index) {
            Dss::Core::TrackResultEvent event;
            event.targets.resize(1);
            event.targets.front().targetId = std::to_string(index);
            event.targets.front().living = true;
            event.targets.front().frameInfos.resize(1000);
            bus.emit(event);
        }
    });
    worker.join();
    EXPECT_TRUE(updates.empty());
    app.processEvents();
    ASSERT_EQ(updates.size(), 1U);
    EXPECT_TRUE(updates.front().contains("Target: 99 |"));
}

TEST(TrackingViewModel, DisplaysOnlyLivingTargetsAndClearsOnEmptySnapshot) {
    (void)ensureApplication();
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::TrackingViewModel tracking({.bus = bus, .registry = registry});
    std::vector<int> counts;
    std::vector<QString> infos;
    QObject::connect(&tracking, &Dss::Ui::TrackingViewModel::targetListUpdated, &tracking,
                     [&](int count) { counts.push_back(count); });
    QObject::connect(&tracking, &Dss::Ui::TrackingViewModel::trackInfoUpdated, &tracking,
                     [&](const QString& info) { infos.push_back(info); });
    Dss::Core::TargetInfo retired;
    retired.targetId = "retired";
    Dss::Core::TargetInfo active;
    active.targetId = "active";
    active.living = true;
    bus.emit(Dss::Core::TrackResultEvent{.frameSeq = 1, .targets = {retired, active}});
    bus.emit(Dss::Core::TrackResultEvent{.frameSeq = 2, .targets = {retired}});
    bus.emit(Dss::Core::TrackResultEvent{.frameSeq = 3, .targets = {}});
    EXPECT_EQ(counts, (std::vector<int>{1, 0, 0}));
    ASSERT_EQ(infos.size(), 3U);
    EXPECT_TRUE(infos[0].contains("Target: active |"));
    EXPECT_TRUE(infos[1].isEmpty());
    EXPECT_TRUE(infos[2].isEmpty());
}
