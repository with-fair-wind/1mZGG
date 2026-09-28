#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QTemporaryDir>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/app/replay_session.h"
#include "dss/core/event/events.h"
#include "dss/core/event/message_bus.h"
#include "dss/core/service/service_registry.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/processing/strategy/diff_processing_strategy.h"
#include "dss/ui/view_model/replay_view_model.h"
#include "dss/ui/view_model/view_model_context.h"

namespace {

class QCoreApplicationFixture {
public:
    QCoreApplicationFixture() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char appName[] = "test_replay_view_model";
            static char* argv[] = {appName, nullptr};
            m_app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> m_app;
};

using MessageBus = Dss::Core::MessageBus;

[[nodiscard]] auto tempReplayViewModelDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_replay_view_model_test";
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto writeGrayBmp(const std::filesystem::path& path, std::uint8_t seed) -> bool {
    QImage image(2, 2, QImage::Format_Grayscale8);
    for (int y = 0; y < image.height(); ++y) {
        auto* row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            row[x] = static_cast<uchar>(seed + static_cast<std::uint8_t>(x + y * image.width()));
        }
    }
    return image.save(QString::fromStdWString(path.wstring()), "BMP");
}

template <typename Predicate>
[[nodiscard]] auto waitForQt(Predicate predicate,
                             std::chrono::milliseconds timeout = std::chrono::seconds{2}) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

}  // namespace

namespace {
class ReplaySessionTest : public ::testing::Test {
protected:
    QCoreApplicationFixture app;
    QTemporaryDir directory;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource> source =
        std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    std::shared_ptr<Dss::Processing::ImageProcessor> processor =
        std::make_shared<Dss::Processing::ImageProcessor>(bus);
    std::unique_ptr<Dss::Ui::ReplayViewModel> replay;
    std::vector<std::uint64_t> processed;
    Dss::Evt::ScopedConnection connection;
    QString status;

    void SetUp() override {
        ASSERT_TRUE(directory.isValid());
        source->setFrameInterval(std::chrono::milliseconds{0});
        source->setFrameCallback([processor = processor](auto packet, auto context) {
            return processor->submitFrameBlocking(std::move(packet), context.stopToken);
        });
        registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                             source);
        registry.registerService<Dss::Acquisition::IFrameSource>("frame_source", source);
        registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
        registry.registerService<Dss::App::ReplaySession>(
            "replay_session", std::make_shared<Dss::App::ReplaySession>(bus, source, processor));
        replay =
            std::make_unique<Dss::Ui::ReplayViewModel>(Dss::Ui::UiServiceContext{bus, registry});
        QObject::connect(replay.get(), &Dss::Ui::ReplayViewModel::statusTextChanged, replay.get(),
                         [this](const QString& text) { status = text; });
        connection = bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
            [this](const auto& event) { processed.push_back(event.frameSeq); });
    }

    void TearDown() override {
        replay->shutdown();
        connection.disconnect();
        replay.reset();
    }

    QString frame(const char* name, std::uint8_t seed) {
        const auto path = directory.filePath(QString::fromUtf8(name));
        EXPECT_TRUE(writeGrayBmp(std::filesystem::path(path.toStdWString()), seed));
        return path;
    }

    bool step() {
        if (!replay->stepReplayForward() || !waitForQt([&] { return !replay->replayBusy(); })) {
            return false;
        }
        processor->drain();
        QCoreApplication::processEvents();
        return true;
    }
};

class BlobCountTracker final : public Dss::Tracking::ITrackingStrategy {
public:
    explicit BlobCountTracker(std::shared_ptr<std::vector<std::size_t>> counts)
        : m_counts(std::move(counts)) {}
    auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Geo;
    }
    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override {
        m_counts->push_back(measurements.targetBlobs.size());
        return {};
    }
    void reset() override {}

private:
    std::shared_ptr<std::vector<std::size_t>> m_counts;
};
}  // namespace

TEST_F(ReplaySessionTest, NaturalCompletionDrainsFramesAndAllowsRestart) {
    ASSERT_TRUE(replay->selectReplayFiles({frame("first.bmp", 10), frame("second.bmp", 50)}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    replay->startGrab();
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy() && !replay->isGrabbing(); }));
    EXPECT_EQ(status, "Replay finished");
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{0, 1}));
    EXPECT_FALSE(processor->isRunning());

    replay->startGrab();
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy() && !replay->isGrabbing(); }));
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{0, 1, 0, 1}));
}

TEST_F(ReplaySessionTest, BadMiddleFrameReportsFailureAndStopsUi) {
    const auto missing = directory.filePath("missing.bmp");
    ASSERT_TRUE(
        replay->selectReplayFiles({frame("first.bmp", 10), missing, frame("last.bmp", 90)}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    replay->startGrab();
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy() && !replay->isGrabbing(); }));
    EXPECT_TRUE(status.startsWith("Replay failed:"));
    EXPECT_TRUE(status.contains("missing.bmp"));
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{0}));
    EXPECT_FALSE(processor->isRunning());
}

TEST_F(ReplaySessionTest, SeekBackwardAndNewSequenceResetDifferenceHistory) {
    auto counts = std::make_shared<std::vector<std::size_t>>();
    processor->setProcessingStrategy(std::make_unique<Dss::Processing::DiffProcessingStrategy>(
        Dss::Processing::DiffProcessingOptions{.threshold = 5, .minArea = 1}));
    processor->setTrackingStrategy(std::make_unique<BlobCountTracker>(counts));
    const auto first = frame("first.bmp", 10);
    const auto second = frame("second.bmp", 90);
    ASSERT_TRUE(replay->selectReplayFiles({first, second}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    ASSERT_TRUE(step());
    ASSERT_TRUE(step());
    ASSERT_EQ(*counts, (std::vector<std::size_t>{0, 1}));
    ASSERT_TRUE(replay->seekReplayFrame(0));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    ASSERT_TRUE(step());
    EXPECT_EQ(counts->back(), 0U);
    ASSERT_TRUE(step());
    EXPECT_EQ(counts->back(), 1U);
    ASSERT_TRUE(replay->stepReplayBackward());
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    processor->drain();
    EXPECT_EQ(counts->back(), 0U);
    ASSERT_TRUE(replay->selectReplayFiles({second}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    ASSERT_TRUE(step());
    EXPECT_EQ(counts->back(), 0U);
}

TEST_F(ReplaySessionTest, ShutdownCancelsTaskAndIgnoresQueuedCompletion) {
    ASSERT_TRUE(replay->selectReplayFiles({frame("first.bmp", 10)}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    replay->startGrab();
    replay->shutdown();
    QCoreApplication::processEvents();
    EXPECT_FALSE(replay->isGrabbing());
    EXPECT_FALSE(replay->replayBusy());
    EXPECT_FALSE(source->isRunning());
    EXPECT_FALSE(processor->isRunning());
    replay->startGrab();
    EXPECT_FALSE(replay->replayBusy());
    EXPECT_FALSE(replay->stepReplayForward());
}

TEST_F(ReplaySessionTest, RejectsCommandsUntilAcceptedSelectionIsReflectedOnUiThread) {
    const auto path = frame("first.bmp", 10);
    ASSERT_TRUE(replay->selectReplayFiles({path}));
    EXPECT_TRUE(replay->replayBusy());
    EXPECT_FALSE(replay->stepReplayForward());
    EXPECT_FALSE(replay->seekReplayFrame(0));
    EXPECT_FALSE(replay->selectReplayFiles({path}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    EXPECT_EQ(replay->replayFrameCount(), 1);
    EXPECT_EQ(status, "Sequence selected: 1 frames");
    ASSERT_TRUE(step());
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{0}));
}

TEST(ReplayViewModel, UpdatesCurrentReplayFrameFromDisplayEvents) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});

    EXPECT_EQ(replay.replayCurrentFrame(), 0);

    auto image =
        std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1, 2, 3, 4});
    bus.emit(Dss::Core::DisplayRefreshEvent{4, 2, 2, 2, std::move(image), nullptr});

    EXPECT_EQ(replay.replayCurrentFrame(), 5);
}

TEST(ReplayViewModel, SelectsSequenceAndStepsForward) {
    QCoreApplicationFixture app;
    const auto dir = tempReplayViewModelDir();
    const auto first = dir / "replay_vm_0001.bmp";
    const auto second = dir / "replay_vm_0002.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    ASSERT_TRUE(writeGrayBmp(second, 40));

    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    replaySource->setFrameCallback([&bus](Dss::Processing::FramePacket packet,
                                          Dss::Acquisition::FrameDeliveryContext) {
        auto image =
            std::make_shared<const std::vector<std::uint8_t>>(std::move(packet.displayImage));
        auto raw = packet.rawImage;
        bus.emit(Dss::Core::DisplayRefreshEvent{packet.frameSeq, packet.width, packet.height,
                                                packet.width, std::move(image), std::move(raw)});
        return true;
    });
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);

    registry.registerService<Dss::App::ReplaySession>(
        "replay_session", std::make_shared<Dss::App::ReplaySession>(bus, replaySource, processor));
    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    std::vector<bool> busyStates;
    const auto busyConnection =
        QObject::connect(&replay, &Dss::Ui::ReplayViewModel::replayBusyChanged,
                         [&busyStates](bool busy) { busyStates.push_back(busy); });

    ASSERT_TRUE(replay.selectReplayFiles(QStringList{QString::fromStdWString(first.wstring()),
                                                     QString::fromStdWString(second.wstring())}));
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_FALSE(replay.replayBusy());
    EXPECT_EQ(replay.replayFrameCount(), 2);
    EXPECT_EQ(replay.replayCurrentFrame(), 0);
    EXPECT_EQ(replaySource->frameWidth(), 0U);
    EXPECT_EQ(replaySource->frameHeight(), 0U);
    EXPECT_EQ(busyStates, (std::vector<bool>{true, false}));

    EXPECT_TRUE(replay.stepReplayForward());
    EXPECT_TRUE(replay.replayBusy());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_EQ(replay.replayCurrentFrame(), 1);
    EXPECT_EQ(replaySource->frameWidth(), 2U);
    EXPECT_EQ(replaySource->frameHeight(), 2U);

    EXPECT_TRUE(replay.stepReplayForward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_EQ(replay.replayCurrentFrame(), 2);

    EXPECT_TRUE(replay.stepReplayBackward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_EQ(replay.replayCurrentFrame(), 1);

    EXPECT_TRUE(replay.seekReplayFrame(1));
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_TRUE(replay.stepReplayForward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_EQ(replay.replayCurrentFrame(), 2);

    EXPECT_TRUE(replay.stepReplayForward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_EQ(replay.replayCurrentFrame(), 2);

    QObject::disconnect(busyConnection);
}

TEST(ReplayViewModel, DefersMissingReplayFileErrorUntilStep) {
    QCoreApplicationFixture app;
    const auto missing = tempReplayViewModelDir() / "missing_replay_frame.bmp";
    std::error_code removeError;
    std::filesystem::remove(missing, removeError);

    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    replaySource->setFrameCallback(
        [](Dss::Processing::FramePacket, Dss::Acquisition::FrameDeliveryContext) { return true; });
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);

    registry.registerService<Dss::App::ReplaySession>(
        "replay_session", std::make_shared<Dss::App::ReplaySession>(bus, replaySource, processor));
    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_TRUE(replay.selectReplayFiles({QString::fromStdWString(missing.wstring())}));
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_FALSE(replay.replayBusy());
    EXPECT_EQ(replay.replayFrameCount(), 1);
    EXPECT_TRUE(statusText.startsWith("Sequence selected:"));

    ASSERT_TRUE(replay.stepReplayForward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_TRUE(statusText.startsWith("Replay failed:"));
    EXPECT_EQ(replay.replayCurrentFrame(), 0);
}

TEST(ReplayViewModel, StartsReplayInitializationAsBackgroundTask) {
    QCoreApplicationFixture app;
    const auto missing = tempReplayViewModelDir() / "missing_start_frame.bmp";
    std::error_code removeError;
    std::filesystem::remove(missing, removeError);

    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    replaySource->setFrameCallback(
        [](Dss::Processing::FramePacket, Dss::Acquisition::FrameDeliveryContext) { return true; });
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);
    registry.registerService<Dss::Acquisition::IFrameSource>("frame_source", replaySource);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);

    registry.registerService<Dss::App::ReplaySession>(
        "replay_session", std::make_shared<Dss::App::ReplaySession>(bus, replaySource, processor));
    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });
    ASSERT_TRUE(replay.selectReplayFiles({QString::fromStdWString(missing.wstring())}));
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));

    replay.startGrab();

    EXPECT_TRUE(replay.replayBusy());
    EXPECT_FALSE(replay.isGrabbing());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_FALSE(replay.isGrabbing());
    EXPECT_TRUE(statusText.startsWith("Replay failed:"));
}

TEST(ReplayViewModel, StartsReplayAfterBackgroundInitializationSucceeds) {
    QCoreApplicationFixture app;
    const auto frame = tempReplayViewModelDir() / "start_replay_frame.bmp";
    ASSERT_TRUE(writeGrayBmp(frame, 20));

    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    replaySource->setFrameInterval(std::chrono::seconds{2});
    replaySource->setFrameCallback(
        [](Dss::Processing::FramePacket, Dss::Acquisition::FrameDeliveryContext) { return true; });
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);
    registry.registerService<Dss::Acquisition::IFrameSource>("frame_source", replaySource);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);

    registry.registerService<Dss::App::ReplaySession>(
        "replay_session", std::make_shared<Dss::App::ReplaySession>(bus, replaySource, processor));
    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });
    ASSERT_TRUE(replay.selectReplayFiles(
        {QString::fromStdWString(frame.wstring()), QString::fromStdWString(frame.wstring())}));
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));

    replay.startGrab();

    EXPECT_TRUE(replay.replayBusy());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_TRUE(replay.isGrabbing());
    EXPECT_EQ(replaySource->frameWidth(), 2U);
    EXPECT_EQ(replaySource->frameHeight(), 2U);
    EXPECT_EQ(statusText, "Replaying...");
    replay.stopGrab();
}

TEST_F(ReplaySessionTest, LastFrameProcessingFailureIsNotReportedAsSuccessfulEof) {
    class Failure final : public Dss::Processing::IProcessingStrategy {
    public:
        auto process(const Dss::Processing::FramePacket&)
            -> Dss::Processing::ProcessingResult override {
            throw std::runtime_error("last frame processing failure");
        }
        auto name() const -> std::string_view override {
            return "failure";
        }
        auto mode() const -> Dss::Core::ProcessingMode override {
            return Dss::Core::ProcessingMode::Direct;
        }
    };
    ASSERT_TRUE(replay->selectReplayFiles({frame("last.bmp", 10)}));
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy(); }));
    processor->setProcessingStrategy(std::make_unique<Failure>());
    replay->startGrab();
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy() && !replay->isGrabbing(); }));
    EXPECT_TRUE(processor->hasFailed());
    EXPECT_TRUE(status.startsWith("Replay failed:"));
    processor->setProcessingStrategy(nullptr);
    replay->startGrab();
    ASSERT_TRUE(waitForQt([&] { return !replay->replayBusy() && !replay->isGrabbing(); }));
    EXPECT_FALSE(processor->hasFailed());
    EXPECT_EQ(status, "Replay finished");
}
