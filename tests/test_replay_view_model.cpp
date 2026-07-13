#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/core/event/events.h"
#include "dss/core/event/message_bus.h"
#include "dss/core/service/service_registry.h"
#include "dss/processing/pipeline/image_processor.h"
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

    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    std::vector<bool> busyStates;
    const auto busyConnection =
        QObject::connect(&replay, &Dss::Ui::ReplayViewModel::replayBusyChanged,
                         [&busyStates](bool busy) { busyStates.push_back(busy); });

    ASSERT_TRUE(replay.selectReplayFiles(QStringList{QString::fromStdWString(first.wstring()),
                                                     QString::fromStdWString(second.wstring())}));
    EXPECT_FALSE(replay.replayBusy());
    EXPECT_EQ(replay.replayFrameCount(), 2);
    EXPECT_EQ(replay.replayCurrentFrame(), 0);
    EXPECT_EQ(replaySource->frameWidth(), 0U);
    EXPECT_EQ(replaySource->frameHeight(), 0U);
    EXPECT_TRUE(busyStates.empty());

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

    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });

    EXPECT_TRUE(replay.selectReplayFiles({QString::fromStdWString(missing.wstring())}));
    EXPECT_FALSE(replay.replayBusy());
    EXPECT_EQ(replay.replayFrameCount(), 1);
    EXPECT_TRUE(statusText.startsWith("Sequence selected:"));

    ASSERT_TRUE(replay.stepReplayForward());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_TRUE(statusText.startsWith("failed to"));
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

    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });
    ASSERT_TRUE(replay.selectReplayFiles({QString::fromStdWString(missing.wstring())}));

    replay.startGrab();

    EXPECT_TRUE(replay.replayBusy());
    EXPECT_FALSE(replay.isGrabbing());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_FALSE(replay.isGrabbing());
    EXPECT_TRUE(statusText.startsWith("failed to"));
}

TEST(ReplayViewModel, StartsReplayAfterBackgroundInitializationSucceeds) {
    QCoreApplicationFixture app;
    const auto frame = tempReplayViewModelDir() / "start_replay_frame.bmp";
    ASSERT_TRUE(writeGrayBmp(frame, 20));

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

    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    QString statusText;
    QObject::connect(&replay, &Dss::Ui::ReplayViewModel::statusTextChanged,
                     [&statusText](const QString& text) { statusText = text; });
    ASSERT_TRUE(replay.selectReplayFiles({QString::fromStdWString(frame.wstring())}));

    replay.startGrab();

    EXPECT_TRUE(replay.replayBusy());
    ASSERT_TRUE(waitForQt([&] { return !replay.replayBusy(); }));
    EXPECT_TRUE(replay.isGrabbing());
    EXPECT_EQ(replaySource->frameWidth(), 2U);
    EXPECT_EQ(replaySource->frameHeight(), 2U);
    EXPECT_EQ(statusText, "Replaying...");
    replay.stopGrab();
}
