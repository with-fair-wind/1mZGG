#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "dss/storage/backend/local_image_storage_backend.h"

namespace {

[[nodiscard]] auto tempStorageDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_local_image_storage_backend_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto metadata() -> Dss::Storage::RawImageMetadata {
    Dss::Storage::RawImageMetadata meta{};
    meta.width = 2;
    meta.height = 1;
    meta.exposure.timestamp = {
        .year = 2026,
        .month = 6,
        .day = 3,
        .hour = 8,
        .minute = 9,
        .second = 10,
        .millisecond = 11,
        .microsecond = 12,
    };
    meta.exposureTimeMilliseconds = 4.0;
    meta.frameFrequency = 25.0;
    return meta;
}

[[nodiscard]] auto readAllBytes(const std::filesystem::path& path) -> std::vector<std::uint8_t> {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(LocalImageStorageBackend, RejectsEnqueueBeforeWorkerStarts) {
    auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());

    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};

    EXPECT_FALSE(backend.enqueueRawFrame("frame.raw", metadata(), pixels).has_value());
}

TEST(LocalImageStorageBackend, RejectsEnqueueWhenQueueIsFullAndCountsDrop) {
    auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir, 0);
    ASSERT_TRUE(backend.init(dir).has_value());
    ASSERT_TRUE(backend.start().has_value());

    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};

    EXPECT_FALSE(backend.enqueueRawFrame("frame.raw", metadata(), pixels).has_value());
    EXPECT_EQ(backend.droppedRequests(), 1U);

    backend.stop();
}
TEST(LocalImageStorageBackend, DrainsRawFrameWritesWhenStopped) {
    auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());
    ASSERT_TRUE(backend.start().has_value());

    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};
    ASSERT_TRUE(backend.enqueueRawFrame("frames/frame_000001.raw", metadata(), pixels).has_value());

    backend.stop();

    const auto outputPath = dir / "frames" / "frame_000001.raw";
    ASSERT_TRUE(std::filesystem::exists(outputPath));

    const auto decoded = Dss::Storage::decodeRawImageFile(readAllBytes(outputPath));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->metadata.width, 2U);
    EXPECT_EQ(decoded->metadata.height, 1U);
    EXPECT_EQ(decoded->metadata.exposure.timestamp.year, 2026);
    EXPECT_EQ(decoded->pixels, pixels);
}
TEST(LocalImageStorageBackend, WritesLegacyBmpSessionAndIndex) {
    const auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());

    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "20260619080000";
    naming.endTime = "20260619090000";
    naming.taskId = "task-1";
    naming.targetId = "42";
    naming.observatoryId = "7";
    naming.imageFormat = "bmp";
    ASSERT_TRUE(backend.configureSession(naming).has_value());
    ASSERT_TRUE(backend.start().has_value());

    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};
    ASSERT_TRUE(backend.enqueueSessionFrame(3U, metadata(), pixels).has_value());
    backend.stop();

    naming.rootPath = dir;
    const auto imagePath = Dss::Storage::buildImageFilePath(naming, metadata(), 3U);
    ASSERT_TRUE(std::filesystem::exists(imagePath));
    EXPECT_TRUE(Dss::Storage::decodeLegacyBmpFile(readAllBytes(imagePath)).has_value());
    EXPECT_TRUE(std::filesystem::exists(dir / Dss::Storage::buildIfmFileName(naming)));
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.failedWrites(), 0U);
}

TEST(LocalImageStorageBackend, PublishesWriteErrors) {
    const auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend::MessageBus bus;
    std::vector<Dss::Core::StorageWriteErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::StorageWriteErrorEvent>(
        [&errors](const auto& event) { errors.push_back(event); });

    Dss::Storage::LocalImageStorageBackend backend(dir);
    backend.setBus(&bus);
    ASSERT_TRUE(backend.init(dir).has_value());
    ASSERT_TRUE(backend.start().has_value());
    std::ofstream(dir / "blocked") << "file";

    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};
    ASSERT_TRUE(backend.enqueueRawFrame("blocked/frame.raw", metadata(), pixels).has_value());
    backend.stop();

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().backend, "image_storage");
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.successfulWrites(), 0U);
    EXPECT_EQ(backend.resourceSnapshot().completedItems, 1U);
    EXPECT_EQ(backend.pendingBytes(), 0U);

    ASSERT_TRUE(std::filesystem::remove(dir / "blocked"));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueRawFrame("blocked/frame.raw", metadata(), pixels));
    backend.stop();
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.resourceSnapshot().completedItems, 2U);
    EXPECT_EQ(backend.pendingBytes(), 0U);
    const auto decoded = Dss::Storage::decodeRawImageFile(readAllBytes(dir / "blocked/frame.raw"));
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->pixels, pixels);
}

TEST(LocalImageStorageBackend, RejectsOldGenerationAfterSessionSwitch) {
    const auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "first";
    naming.targetId = "1";
    ASSERT_TRUE(backend.configureSession(naming));
    ASSERT_TRUE(backend.start());
    const auto oldGeneration = backend.sessionGeneration();
    backend.requestStop();
    EXPECT_FALSE(backend.configureSession(naming));
    backend.stop();
    naming.startTime = "second";
    ASSERT_TRUE(backend.configureSession(naming));
    ASSERT_TRUE(backend.start());
    auto pixels = std::make_shared<const std::vector<std::uint16_t>>(2, 42);
    EXPECT_FALSE(backend.enqueueSessionFrame(1, metadata(), pixels, oldGeneration));
    EXPECT_TRUE(backend.enqueueSessionFrame(2, metadata(), pixels, backend.sessionGeneration()));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
}

TEST(LocalImageStorageBackend, RejectsFrameLargerThanByteBudget) {
    const auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir, 1024, 3);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    EXPECT_FALSE(
        backend.enqueueRawFrame("frame.raw", metadata(), std::vector<std::uint16_t>{1, 2}));
    EXPECT_EQ(backend.droppedRequests(), 1U);
    EXPECT_EQ(backend.pendingBytes(), 0U);
    backend.stop();
}

#ifdef __linux__
TEST(LocalImageStorageBackend, ReportsBufferedCloseFailureOnFullDevice) {
    const auto dir = tempStorageDir();
    Dss::Core::MessageBus bus;
    std::vector<Dss::Core::StorageWriteErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::StorageWriteErrorEvent>(
        [&errors](const auto& event) { errors.push_back(event); });
    Dss::Storage::LocalImageStorageBackend backend(dir);
    backend.setBus(&bus);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(std::filesystem::exists("/dev/full"));
    const auto outputPath = dir / "frame.raw";
    std::filesystem::create_symlink("/dev/full", outputPath);
    ASSERT_TRUE(backend.start());
    // 小图可先进入流缓冲；关闭时才暴露的 ENOSPC 也必须计为失败。
    ASSERT_TRUE(backend.enqueueRawFrame("frame.raw", metadata(),
                                        std::vector<std::uint16_t>{0x1234, 0xABCD}));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 0U);
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.resourceSnapshot().completedItems, 1U);
    EXPECT_EQ(backend.resourceSnapshot().queuedItems, 0U);
    EXPECT_EQ(backend.resourceSnapshot().activeItems, 0U);
    EXPECT_EQ(backend.pendingBytes(), 0U);
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().backend, "image_storage");
    EXPECT_EQ(errors.front().path, outputPath.string());
    EXPECT_EQ(errors.front().message, "failed to write or close storage file");
}

TEST(LocalImageStorageBackend, PreservesSessionWhenIndexCloseFailsAndAllowsRetry) {
    const auto dir = tempStorageDir();
    Dss::Storage::LocalImageStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "first";
    naming.targetId = "1";
    ASSERT_TRUE(backend.configureSession(naming));
    const auto previousPath = backend.sessionPath();
    const auto previousGeneration = backend.sessionGeneration();
    naming.startTime = "second";
    const auto indexPath = dir / Dss::Storage::buildIfmFileName(naming);
    ASSERT_TRUE(std::filesystem::exists("/dev/full"));
    std::filesystem::create_symlink("/dev/full", indexPath);
    const auto result = backend.configureSession(naming);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(),
              "failed to write or close image session index: " + indexPath.string());
    EXPECT_EQ(backend.sessionPath(), previousPath);
    EXPECT_EQ(backend.sessionGeneration(), previousGeneration);

    ASSERT_TRUE(std::filesystem::remove(indexPath));
    ASSERT_TRUE(backend.configureSession(naming));
    EXPECT_NE(backend.sessionPath(), previousPath);
    EXPECT_EQ(backend.sessionGeneration(), previousGeneration + 1);
    naming.rootPath = dir;
    const auto bytes = readAllBytes(indexPath);
    const auto expected = Dss::Storage::buildIfmContent(naming);
    EXPECT_EQ(bytes, std::vector<std::uint8_t>(expected.begin(), expected.end()));
}
#endif
