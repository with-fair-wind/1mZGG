#include <QCoreApplication>
#include <QImage>
#include <QTemporaryDir>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/core/event/events.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/storage/format/bmp_image_format.h"
#include "dss/storage/format/image_storage_format.h"
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

class SlowReplayStrategy final : public Dss::Processing::IProcessingStrategy {
public:
    [[nodiscard]] auto process(const Dss::Processing::FramePacket&)
        -> Dss::Processing::ProcessingResult override {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        Dss::Processing::ProcessingResult result;
        result.success = true;
        return result;
    }

    [[nodiscard]] auto name() const -> std::string_view override {
        return "slow_replay";
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }
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
    std::vector<Dss::Acquisition::FrameDeliveryPolicy> policies;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext context) {
            {
                std::lock_guard lock(mutex);
                frames.push_back(std::move(packet));
                policies.push_back(context.policy);
            }
            cv.notify_one();
            return true;
        });

    source.start();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            cv.wait_for(lock, std::chrono::seconds{2}, [&] { return frames.size() == 2U; }));
    }
    source.stop();

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(policies, (std::vector<Dss::Acquisition::FrameDeliveryPolicy>{
                            Dss::Acquisition::FrameDeliveryPolicy::Lossless,
                            Dss::Acquisition::FrameDeliveryPolicy::Lossless}));
    EXPECT_EQ(frames[0].frameSeq, 0U);
    EXPECT_EQ(frames[1].frameSeq, 1U);
    EXPECT_EQ(frames[0].width, 3U);
    EXPECT_EQ(frames[0].height, 2U);
    EXPECT_EQ(frames[0].displayImage.size(), 6U);
    ASSERT_TRUE(frames[0].rawImage);
    EXPECT_EQ(frames[0].rawImage->size(), 6U);
    EXPECT_EQ(frames[0].displayImage[0], 10U);
    EXPECT_EQ(frames[1].displayImage[0], 40U);
}

TEST(ImageSequenceFrameSource, LosslessReplayWaitsUntilEveryFrameIsProcessed) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    std::vector<std::filesystem::path> files;
    for (int index = 0; index < 8; ++index) {
        const auto file = dir / ("lossless_" + std::to_string(index) + ".bmp");
        ASSERT_TRUE(writeGrayBmp(file, static_cast<std::uint8_t>(10 + index)));
        files.push_back(file);
    }

    Dss::Core::MessageBus bus;
    Dss::Processing::ImageProcessor processor(bus);
    processor.setProcessingStrategy(std::make_unique<SlowReplayStrategy>());

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::uint64_t> completedFrames;
    auto connection = bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [&](const Dss::Core::ProcessingCompleteEvent& event) {
            {
                std::lock_guard lock(mutex);
                completedFrames.push_back(event.frameSeq);
            }
            cv.notify_one();
        });

    Dss::Acquisition::ImageSequenceFrameSource source(files);
    source.setFrameInterval(std::chrono::milliseconds{0});
    ASSERT_TRUE(source.init().has_value());
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext context) {
            if (context.policy != Dss::Acquisition::FrameDeliveryPolicy::Lossless) {
                return false;
            }
            return processor.submitFrameBlocking(std::move(packet), context.stopToken);
        });

    processor.start();
    source.start();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds{3},
                                [&] { return completedFrames.size() == files.size(); }));
    }
    source.stop();
    processor.stop();

    EXPECT_EQ(completedFrames, (std::vector<std::uint64_t>{0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U}));
    EXPECT_EQ(source.nextFrameIndex(), files.size());
    EXPECT_EQ(processor.droppedFrames(), 0U);
}

TEST(ImageSequenceFrameSource, StopsAtUnreadableFrameInsteadOfSkippingIt) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "unreadable_guard_0001.bmp";
    const auto missing = dir / "unreadable_guard_0002.bmp";
    const auto third = dir / "unreadable_guard_0003.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    std::filesystem::remove(missing);
    ASSERT_TRUE(writeGrayBmp(third, 30));

    Dss::Acquisition::ImageSequenceFrameSource source({first, missing, third});
    source.setFrameInterval(std::chrono::milliseconds{0});
    ASSERT_TRUE(source.init().has_value());

    std::mutex mutex;
    std::vector<std::uint64_t> frames;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            std::lock_guard lock(mutex);
            frames.push_back(packet.frameSeq);
            return true;
        });

    source.start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (source.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    source.stop();

    std::lock_guard lock(mutex);
    EXPECT_EQ(frames, std::vector<std::uint64_t>({0U}));
    EXPECT_EQ(source.nextFrameIndex(), 1U);
}

TEST(ImageSequenceFrameSource, CanceledSubmissionDoesNotAdvanceReplayPosition) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "canceled_submission_0001.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));

    Dss::Acquisition::ImageSequenceFrameSource source({first});
    ASSERT_TRUE(source.init().has_value());
    source.setFrameCallback(
        [](Dss::Processing::FramePacket, Dss::Acquisition::FrameDeliveryContext) { return false; });

    EXPECT_FALSE(source.stepForward().has_value());
    EXPECT_EQ(source.nextFrameIndex(), 0U);
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
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            {
                std::lock_guard lock(mutex);
                frames.push_back(std::move(packet));
            }
            cv.notify_one();
            return true;
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

TEST(ImageSequenceFrameSource, InitDoesNotRewindAlreadyInitializedSequence) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "idempotent_init_0001.bmp";
    const auto second = dir / "idempotent_init_0002.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    ASSERT_TRUE(writeGrayBmp(second, 40));

    Dss::Acquisition::ImageSequenceFrameSource source;
    ASSERT_TRUE(source.setFiles({first, second}).has_value());
    ASSERT_TRUE(source.init().has_value());

    std::vector<std::uint64_t> frames;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            frames.push_back(packet.frameSeq);
            return true;
        });
    ASSERT_TRUE(source.stepForward().has_value());
    EXPECT_EQ(source.nextFrameIndex(), 1U);

    ASSERT_TRUE(source.init().has_value());
    EXPECT_EQ(source.nextFrameIndex(), 1U);
    ASSERT_TRUE(source.stepForward().has_value());
    EXPECT_EQ(frames, std::vector<std::uint64_t>({0U, 1U}));
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
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            frames.push_back(std::move(packet));
            return true;
        });

    ASSERT_TRUE(source.stepForward().has_value());

    ASSERT_EQ(frames.size(), 1U);
    const std::vector<std::uint16_t> expectedRaw{1000, 1000, 1500, 2000};
    EXPECT_EQ(frames.front().width, 2U);
    EXPECT_EQ(frames.front().height, 2U);
    ASSERT_TRUE(frames.front().rawImage);
    EXPECT_EQ(*frames.front().rawImage, expectedRaw);
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

TEST(ImageSequenceFrameSource, SeekChangesNextFrameWithoutPlayingWhilePaused) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    const auto first = dir / "seek_0001.bmp";
    const auto second = dir / "seek_0002.bmp";
    ASSERT_TRUE(writeGrayBmp(first, 10));
    ASSERT_TRUE(writeGrayBmp(second, 40));

    Dss::Acquisition::ImageSequenceFrameSource source({first, second});
    ASSERT_TRUE(source.init().has_value());
    std::vector<std::uint64_t> frames;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            frames.push_back(packet.frameSeq);
            return true;
        });

    ASSERT_TRUE(source.seek(1).has_value());
    EXPECT_EQ(source.nextFrameIndex(), 1U);
    EXPECT_TRUE(frames.empty());
    ASSERT_TRUE(source.stepForward().has_value());
    EXPECT_EQ(frames, std::vector<std::uint64_t>({1U}));
    EXPECT_FALSE(source.seek(2).has_value());
}

TEST(ImageSequenceFrameSource, SeekWhileRunningContinuesAtRequestedFrame) {
    QCoreApplicationFixture app;
    const auto dir = tempSequenceDir();
    std::vector<std::filesystem::path> files;
    for (int index = 0; index < 4; ++index) {
        const auto file = dir / ("running_seek_" + std::to_string(index) + ".bmp");
        ASSERT_TRUE(writeGrayBmp(file, static_cast<std::uint8_t>(10 + index * 20)));
        files.push_back(file);
    }

    Dss::Acquisition::ImageSequenceFrameSource source(files);
    source.setFrameInterval(std::chrono::milliseconds{100});
    ASSERT_TRUE(source.init().has_value());
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::uint64_t> frames;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet, Dss::Acquisition::FrameDeliveryContext) {
            {
                std::lock_guard lock(mutex);
                frames.push_back(packet.frameSeq);
            }
            cv.notify_all();
            return true;
        });

    source.start();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds{2}, [&] { return !frames.empty(); }));
    }
    EXPECT_FALSE(source.seek(4).has_value());
    EXPECT_TRUE(source.isRunning());
    ASSERT_TRUE(source.seek(2).has_value());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (source.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    source.stop();

    std::lock_guard lock(mutex);
    ASSERT_GE(frames.size(), 3U);
    EXPECT_EQ(frames.front(), 0U);
    EXPECT_EQ(frames[frames.size() - 2U], 2U);
    EXPECT_EQ(frames.back(), 3U);
}

TEST(ImageSequenceFrameSource, RejectsOversizedRawDimensionsBeforePayloadRead) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = std::filesystem::path(directory.path().toStdWString()) / "huge.raw";
    Dss::Storage::RawImageMetadata metadata{};
    metadata.width = 65535;
    metadata.height = 65535;
    const auto header = Dss::Storage::buildRawImageHeader(metadata);
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    output.close();
    Dss::Acquisition::ImageSequenceFrameSource source({path});
    const auto result = source.init();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("dimensions or payload length"), std::string::npos);
}
