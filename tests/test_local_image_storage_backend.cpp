#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "dss/storage/local_image_storage_backend.h"

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
    meta.exposure.timestamp = {.year = 2026,
                               .month = 6,
                               .day = 3,
                               .hour = 8,
                               .minute = 9,
                               .second = 10,
                               .millisecond = 11,
                               .microsecond = 12};
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
