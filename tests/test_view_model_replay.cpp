#include <QCoreApplication>
#include <QImage>
#include <QStringList>
#include <QVariant>
#include <filesystem>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/core/events.h"
#include "dss/ui/view_model.h"

namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_view_model_replay";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

}  // namespace

[[nodiscard]] auto tempReplayDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_view_model_replay_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto writeReplayBmp(const std::filesystem::path& path, uint8_t value) -> bool {
    QImage image(2, 2, QImage::Format_Grayscale8);
    image.fill(value);
    return image.save(QString::fromStdWString(path.wstring()), "BMP");
}

TEST(ViewModelReplay, UpdatesCurrentReplayFrameFromDisplayEvents) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_EQ(viewModel.property("replayCurrentFrame").toInt(), 0);

    auto image = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1, 2, 3, 4});
    bus.emit(Dss::Core::DisplayRefreshEvent{4, 2, 2, 2, std::move(image)});

    EXPECT_EQ(viewModel.property("replayCurrentFrame").toInt(), 5);
}

TEST(ViewModelReplay, StepReplayForwardAdvancesSelectedSequenceOneFrameAtATime) {
    auto& app = ensureApplication();
    (void)app;

    const auto dir = tempReplayDir();
    const auto first = dir / "frame_0001.bmp";
    const auto second = dir / "frame_0002.bmp";
    ASSERT_TRUE(writeReplayBmp(first, 10));
    ASSERT_TRUE(writeReplayBmp(second, 40));

    Dss::Ui::ViewModel::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    replaySource->setFrameCallback([&bus](Dss::Processing::FramePacket packet) {
        auto image = std::make_shared<const std::vector<uint8_t>>(std::move(packet.displayImage));
        bus.emit(Dss::Core::DisplayRefreshEvent{packet.frameSeq, packet.width, packet.height,
                                                packet.width, std::move(image)});
    });
    registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                         replaySource);
    Dss::Ui::ViewModel viewModel(bus, registry);

    ASSERT_TRUE(viewModel.selectReplayFiles(QStringList{
        QString::fromStdWString(first.wstring()), QString::fromStdWString(second.wstring())}));
    EXPECT_EQ(viewModel.replayFrameCount(), 2);
    EXPECT_EQ(viewModel.replayCurrentFrame(), 0);

    EXPECT_TRUE(viewModel.stepReplayForward());
    EXPECT_EQ(viewModel.replayCurrentFrame(), 1);

    EXPECT_TRUE(viewModel.stepReplayForward());
    EXPECT_EQ(viewModel.replayCurrentFrame(), 2);

    EXPECT_FALSE(viewModel.stepReplayForward());
    EXPECT_EQ(viewModel.replayCurrentFrame(), 2);
}
