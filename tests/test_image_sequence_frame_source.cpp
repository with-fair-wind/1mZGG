#include <QCoreApplication>
#include <QImage>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/storage/bmp_image_format.h"

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

[[nodiscard]] auto legacyBmpMetadata() -> Dss::Storage::LegacyBmpMetadata {
    Dss::Storage::LegacyBmpMetadata metadata{};
    metadata.width = 2;
    metadata.height = 2;
    metadata.pixelColor = 1;
    metadata.pixelBit = 16;
    metadata.timestamp = {.year = 2026, .month = 6, .day = 16, .hour = 1, .minute = 2, .second = 3};
    metadata.frameRate = 25.0;
    metadata.exposure = 12.5;
    metadata.temperature = 18.0;
    metadata.humidity = 0.45;
    metadata.atmosPressure = 78000.0;
    return metadata;
}

[[nodiscard]] auto writeLegacyBmp(const std::filesystem::path& path) -> bool {
    const std::vector<std::uint8_t> pixels{0xE8, 0x03, 0xE8, 0x03, 0xDC, 0x05, 0xD0, 0x07};
    const auto file = Dss::Storage::buildLegacyBmpFile(legacyBmpMetadata(), pixels);
    if (!file.has_value()) {
        return false;
    }

    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(file->data()),
                 static_cast<std::streamsize>(file->size()));
    return output.good();
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

TEST(ImageSequenceFrameSource, StepForwardAdvancesReplayPositionBeforeContinuousReplay) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "step_0001.bmp";
    const auto second = dir / "step_0002.bmp";
    const auto third = dir / "step_0003.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    ASSERT_TRUE(writeGrayBmp(second, 40));
    ASSERT_TRUE(writeGrayBmp(third, 70));

    Dss::Acquisition::ImageSequenceFrameSource source;
    source.setFrameInterval(std::chrono::milliseconds{0});
    ASSERT_TRUE(source.setFiles({first, second, third}).has_value());
    ASSERT_TRUE(source.init().has_value());

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

    ASSERT_TRUE(source.stepForward().has_value());
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            cv.wait_for(lock, std::chrono::seconds{2}, [&] { return frames.size() == 1U; }));
    }

    source.start();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            cv.wait_for(lock, std::chrono::seconds{2}, [&] { return frames.size() == 3U; }));
    }
    source.stop();

    ASSERT_EQ(frames.size(), 3U);
    EXPECT_EQ(frames[0].frameSeq, 0U);
    EXPECT_EQ(frames[1].frameSeq, 1U);
    EXPECT_EQ(frames[2].frameSeq, 2U);
    EXPECT_EQ(frames[0].displayImage[0], 10U);
    EXPECT_EQ(frames[1].displayImage[0], 40U);
    EXPECT_EQ(frames[2].displayImage[0], 70U);
}

TEST(ImageSequenceFrameSource, ReplaysLegacyBmpWithCustomHeaderAsSixteenBitPixels) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "legacy_0001.bmp";
    ASSERT_TRUE(writeLegacyBmp(first));

    Dss::Acquisition::ImageSequenceFrameSource source;
    source.setFrameInterval(std::chrono::milliseconds{0});
    ASSERT_TRUE(source.setFiles({first}).has_value());
    ASSERT_TRUE(source.init().has_value());
    EXPECT_EQ(source.frameWidth(), 2U);
    EXPECT_EQ(source.frameHeight(), 2U);

    std::vector<Dss::Processing::FramePacket> frames;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet) { frames.push_back(std::move(packet)); });

    ASSERT_TRUE(source.stepForward().has_value());

    ASSERT_EQ(frames.size(), 1U);
    const std::vector<std::uint16_t> expectedRaw{1000, 1000, 1500, 2000};
    EXPECT_EQ(frames.front().width, 2U);
    EXPECT_EQ(frames.front().height, 2U);
    EXPECT_EQ(frames.front().rawImage, expectedRaw);
    EXPECT_TRUE(frames.front().displayImage.empty());
    EXPECT_DOUBLE_EQ(frames.front().stats.minVal, 0.0);
    EXPECT_DOUBLE_EQ(frames.front().stats.maxVal, 0.0);
    EXPECT_FLOAT_EQ(frames.front().metadata.frameFrequency, 25.0F);
    EXPECT_DOUBLE_EQ(frames.front().metadata.temperature, 18.0);
    EXPECT_DOUBLE_EQ(frames.front().metadata.humidity, 0.45);
    EXPECT_DOUBLE_EQ(frames.front().metadata.atmosPressure, 78000.0);
}

TEST(ImageSequenceFrameSource, RejectsEmptySequence) {
    Dss::Acquisition::ImageSequenceFrameSource source;
    EXPECT_FALSE(source.setFiles({}).has_value());
    EXPECT_FALSE(source.init().has_value());
}
