#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/storage/image_storage_format.h"

namespace {

auto sampleMetadata() -> Dss::Storage::RawImageMetadata {
    Dss::Storage::RawImageMetadata metadata{};
    metadata.width = 0x1234;
    metadata.height = 0x0403;
    metadata.exposure.timestamp = {.year = 2026,
                                   .month = 6,
                                   .day = 2,
                                   .hour = 3,
                                   .minute = 4,
                                   .second = 5,
                                   .millisecond = 678,
                                   .microsecond = 901};
    metadata.exposure.pointingAe = {.x = 1.2345F, .y = 6.7890F};
    metadata.exposure.temperature = 17.9;
    metadata.exposure.atmosPressure = 78910.0;
    metadata.exposure.humidity = 0.265;
    metadata.triggerMode = 1;
    metadata.frameFrequency = 25.0;
    metadata.exposureTimeMilliseconds = 12.0;
    return metadata;
}

auto smallImageMetadata() -> Dss::Storage::RawImageMetadata {
    auto metadata = sampleMetadata();
    metadata.width = 2;
    metadata.height = 1;
    return metadata;
}

}  // namespace

TEST(ImageStorageFormat, BuildsLegacyRawHeaderBytes) {
    const auto header = Dss::Storage::buildRawImageHeader(sampleMetadata());

    const std::array<std::uint8_t, Dss::Storage::kRawImageHeaderSize> expected{
        0x12, 0x34,        // width
        0x04, 0x03,        // height
        0x07, 0xEA,        // year
        0x06, 0x02,        // month/day
        0x03, 0x04, 0x05,  // h/m/s
        0x02, 0xA6,        // millisecond
        0x03, 0x85,        // microsecond
        0x00, 0x30, 0x39,  // azimuth * 10000
        0x01, 0x09, 0x32,  // elevation * 10000
        0x00, 0x2E, 0xE0,  // exposure ms -> us
        0x01,              // trigger mode
        0x61, 0xA8,        // frame frequency * 1000
        0x75,              // temperature + 100
        0x1E, 0xD3,        // pressure / 10
        0x1A};             // humidity * 100

    EXPECT_EQ(header, expected);
}

TEST(ImageStorageFormat, BuildsRawImageFileWithLegacyHeaderAndLittleEndianPixels) {
    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};

    const auto file = Dss::Storage::buildRawImageFile(sampleMetadata(), pixels);

    ASSERT_EQ(file.size(), Dss::Storage::kRawImageHeaderSize + 4);
    EXPECT_EQ(file[Dss::Storage::kRawImageHeaderSize + 0], 0x34);
    EXPECT_EQ(file[Dss::Storage::kRawImageHeaderSize + 1], 0x12);
    EXPECT_EQ(file[Dss::Storage::kRawImageHeaderSize + 2], 0xCD);
    EXPECT_EQ(file[Dss::Storage::kRawImageHeaderSize + 3], 0xAB);
}

TEST(ImageStorageFormat, DecodesLegacyRawHeaderForReplay) {
    const auto header = Dss::Storage::buildRawImageHeader(sampleMetadata());

    const auto decoded = Dss::Storage::decodeRawImageHeader(header);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width, 0x1234U);
    EXPECT_EQ(decoded->height, 0x0403U);
    EXPECT_EQ(decoded->exposure.timestamp.year, 2026);
    EXPECT_EQ(decoded->exposure.timestamp.month, 6);
    EXPECT_EQ(decoded->exposure.timestamp.day, 2);
    EXPECT_EQ(decoded->exposure.timestamp.hour, 3);
    EXPECT_EQ(decoded->exposure.timestamp.minute, 4);
    EXPECT_EQ(decoded->exposure.timestamp.second, 5);
    EXPECT_EQ(decoded->exposure.timestamp.millisecond, 678);
    EXPECT_EQ(decoded->exposure.timestamp.microsecond, 901);
    EXPECT_FLOAT_EQ(decoded->exposure.pointingAe.x, 1.2345F);
    EXPECT_FLOAT_EQ(decoded->exposure.pointingAe.y, 6.789F);
    EXPECT_DOUBLE_EQ(decoded->exposure.temperature, 17.0);
    EXPECT_DOUBLE_EQ(decoded->exposure.atmosPressure, 78910.0);
    EXPECT_DOUBLE_EQ(decoded->exposure.humidity, 0.26);
    EXPECT_EQ(decoded->triggerMode, 1);
    EXPECT_DOUBLE_EQ(decoded->frameFrequency, 25.0);
    EXPECT_DOUBLE_EQ(decoded->exposureTimeMilliseconds, 12.0);
}

TEST(ImageStorageFormat, RejectsTruncatedRawHeaderForReplay) {
    const std::array<std::uint8_t, Dss::Storage::kRawImageHeaderSize - 1> truncated{};

    EXPECT_FALSE(Dss::Storage::decodeRawImageHeader(truncated).has_value());
}

TEST(ImageStorageFormat, DecodesLegacyRawImageFilePixelsForReplay) {
    const std::vector<std::uint16_t> pixels{0x1234, 0xABCD};
    const auto file = Dss::Storage::buildRawImageFile(smallImageMetadata(), pixels);

    const auto decoded = Dss::Storage::decodeRawImageFile(file);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->metadata.width, 2U);
    EXPECT_EQ(decoded->metadata.height, 1U);
    ASSERT_EQ(decoded->pixels.size(), 2U);
    EXPECT_EQ(decoded->pixels[0], 0x1234U);
    EXPECT_EQ(decoded->pixels[1], 0xABCDU);
}

TEST(ImageStorageFormat, RejectsTruncatedRawImagePayloadForReplay) {
    const std::vector<std::uint16_t> pixels{0x1234};
    auto file = Dss::Storage::buildRawImageFile(smallImageMetadata(), pixels);
    file.pop_back();

    EXPECT_FALSE(Dss::Storage::decodeRawImageFile(file).has_value());
}

TEST(ImageStorageFormat, ConvertsLegacyFrameFrequencyToReplayInterval) {
    EXPECT_EQ(Dss::Storage::replayIntervalMilliseconds(25.0), 40);
    EXPECT_EQ(Dss::Storage::replayIntervalMilliseconds(0.5), 2000);
    EXPECT_FALSE(Dss::Storage::replayIntervalMilliseconds(0.0).has_value());
    EXPECT_FALSE(Dss::Storage::replayIntervalMilliseconds(-1.0).has_value());

    const std::array<std::uint8_t, 2> frequencyBytes{0x61, 0xA8};
    EXPECT_EQ(Dss::Storage::replayIntervalMilliseconds(frequencyBytes), 40);
}

TEST(ImageStorageFormat, BuildsLegacySessionPathsAndImageNames) {
    Dss::Storage::ImageStorageNaming naming{};
    naming.rootPath = "D:/DPS_Data";
    naming.startTime = "20210101080800";
    naming.targetId = "888888";
    naming.observatoryId = "999";
    naming.imageFormat = "raw";

    const auto sessionPath = Dss::Storage::buildSessionPath(naming);
    EXPECT_EQ(sessionPath.generic_string(),
              "D:/DPS_Data/2021/202101/20210101/20210101080800_888888_999.BMP");

    const auto filePath = Dss::Storage::buildImageFilePath(naming, sampleMetadata(), 7);
    EXPECT_EQ(filePath.generic_string(),
              "D:/DPS_Data/2021/202101/20210101/20210101080800_888888_999.BMP/"
              "20260602030405678_888888_999_000007.raw");

    naming.searchMode = true;
    EXPECT_EQ(Dss::Storage::buildSessionPath(naming).generic_string(),
              "D:/DPS_Data/2021/202101/20210101/20210101080800_NNNNNN_999.BMP");
}

TEST(ImageStorageFormat, BuildsLegacyIfmFileContent) {
    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "20210101080800";
    naming.endTime = "20210101090900";
    naming.taskId = "999999";
    naming.targetId = "888888";
    naming.observatoryId = "999";

    EXPECT_EQ(Dss::Storage::buildIfmFileName(naming), "20210101080800_888888_999.IMI");
    EXPECT_EQ(Dss::Storage::buildIfmContent(naming),
              "C 20210101080800_888888_999.IMI\n"
              "C 999999\n"
              "C 20210101080800\n"
              "C20210101090900\n"
              "C \n"
              "C 999\n"
              "C \n"
              "C \n"
              "C \n"
              "C \n"
              "E\n");

    naming.searchMode = true;
    EXPECT_EQ(Dss::Storage::buildIfmFileName(naming), "20210101080800_NNNNNN_999.IMI");
}
