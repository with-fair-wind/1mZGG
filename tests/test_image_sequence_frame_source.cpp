#include <QCoreApplication>
#include <QImage>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/image_sequence_frame_source.h"

namespace {

class QCoreApplicationFixture {
public:
    QCoreApplicationFixture() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char appName[] = "test_image_sequence_frame_source";
            static char* argv[] = {appName, nullptr};
            m_app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> m_app;
};

[[nodiscard]] auto tempSequenceDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_image_sequence_frame_source_test";
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto writeGrayBmp(const std::filesystem::path& path, uint8_t seed) -> bool {
    QImage image(3, 2, QImage::Format_Grayscale8);
    for (int y = 0; y < image.height(); ++y) {
        auto* row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            row[x] = static_cast<uchar>(seed + static_cast<uint8_t>(x + y * image.width()));
        }
    }
    return image.save(QString::fromStdWString(path.wstring()), "BMP");
}

}  // namespace

TEST(ImageSequenceFrameSource, ReplaysSelectedImageFilesAsFramePackets) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "frame_0001.bmp";
    const auto second = dir / "frame_0002.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    ASSERT_TRUE(writeGrayBmp(second, 40));

    Dss::Acquisition::ImageSequenceFrameSource source;
    source.setFrameInterval(std::chrono::milliseconds{0});
    ASSERT_TRUE(source.setFiles({first, second}).has_value());
    ASSERT_TRUE(source.init().has_value());
    EXPECT_EQ(source.frameWidth(), 3U);
    EXPECT_EQ(source.frameHeight(), 2U);
    EXPECT_EQ(source.frameCount(), 2U);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<Dss::Processing::FramePacket> frames;
    source.setFrameCallback([&](Dss::Processing::FramePacket packet) {
        {
            std::lock_guard lock(mutex);
            frames.push_back(std::move(packet));
        }
        cv.notify_one();
    });

    source.start();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            cv.wait_for(lock, std::chrono::seconds{2}, [&] { return frames.size() == 2U; }));
    }
    source.stop();

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].frameSeq, 0U);
    EXPECT_EQ(frames[1].frameSeq, 1U);
    EXPECT_EQ(frames[0].width, 3U);
    EXPECT_EQ(frames[0].height, 2U);
    EXPECT_EQ(frames[0].displayImage.size(), 6U);
    EXPECT_EQ(frames[0].rawImage.size(), 6U);
    EXPECT_EQ(frames[0].displayImage[0], 10U);
    EXPECT_EQ(frames[1].displayImage[0], 40U);
}

TEST(ImageSequenceFrameSource, RejectsEmptySequence) {
    Dss::Acquisition::ImageSequenceFrameSource source;
    EXPECT_FALSE(source.setFiles({}).has_value());
    EXPECT_FALSE(source.init().has_value());
}
