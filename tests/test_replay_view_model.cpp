#include <QCoreApplication>
#include <QImage>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/ui/replay_view_model.h"
#include "dss/ui/view_model_context.h"

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

using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

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
    replaySource->setFrameCallback([&bus](Dss::Processing::FramePacket packet) {
        auto image =
            std::make_shared<const std::vector<std::uint8_t>>(std::move(packet.displayImage));
        auto raw = std::make_shared<const std::vector<std::uint16_t>>(std::move(packet.rawImage));
        bus.emit(Dss::Core::DisplayRefreshEvent{packet.frameSeq, packet.width, packet.height,
                                                packet.width, std::move(image), std::move(raw)});
    });
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);

    Dss::Ui::ReplayViewModel replay({.bus = bus, .registry = registry});
    ASSERT_TRUE(replay.selectReplayFiles(QStringList{QString::fromStdWString(first.wstring()),
                                                     QString::fromStdWString(second.wstring())}));
    EXPECT_EQ(replay.replayFrameCount(), 2);
    EXPECT_EQ(replay.replayCurrentFrame(), 0);

    EXPECT_TRUE(replay.stepReplayForward());
    EXPECT_EQ(replay.replayCurrentFrame(), 1);

    EXPECT_TRUE(replay.stepReplayForward());
    EXPECT_EQ(replay.replayCurrentFrame(), 2);

    EXPECT_TRUE(replay.stepReplayBackward());
    EXPECT_EQ(replay.replayCurrentFrame(), 1);

    EXPECT_TRUE(replay.seekReplayFrame(1));
    EXPECT_TRUE(replay.stepReplayForward());
    EXPECT_EQ(replay.replayCurrentFrame(), 2);

    EXPECT_FALSE(replay.stepReplayForward());
    EXPECT_EQ(replay.replayCurrentFrame(), 2);
}
