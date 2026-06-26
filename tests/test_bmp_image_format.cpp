#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/storage/bmp_image_format.h"

namespace {

auto attributeByteMetadata() -> Dss::Storage::LegacyBmpMetadata {
    Dss::Storage::LegacyBmpMetadata metadata{};
    metadata.targetId = 0x010203;
    metadata.telescopeId = 0x1234;
    metadata.timestamp = {.year = 2026,
                          .month = 6,
                          .day = 2,
                          .hour = 0,
                          .minute = 0,
                          .second = 1,
                          .millisecond = 0,
                          .microsecond = 0};
    metadata.ztCode = 0x0046;
    metadata.deltaX = 1.2;
    metadata.deltaY = -3.4;
    metadata.pixelScaleX = 10;
    metadata.pixelScaleY = 11;
    metadata.azimuthDegrees = 90.0;
    metadata.elevationDegrees = 45.0;
    metadata.distance = 2.5;
    metadata.focalLength = 123.45;
    metadata.frameRate = 25.5;
    metadata.temperature = 17.9;
    metadata.humidity = 0.26;
    metadata.atmosPressure = 78910.0;
    metadata.windSpeed = 3.4;
    metadata.cloudCover = 1.2;
    metadata.exposure = 12.3;
    return metadata;
}

auto roundTripMetadata() -> Dss::Storage::LegacyBmpMetadata {
    auto metadata = attributeByteMetadata();
    metadata.timestamp.hour = 1;
    metadata.timestamp.minute = 2;
    metadata.timestamp.second = 3;
    metadata.timestamp.millisecond = 4;
    metadata.timestamp.microsecond = 400;
    return metadata;
}

auto smallBmpMetadata(std::uint16_t pixelBit) -> Dss::Storage::LegacyBmpMetadata {
    auto metadata = attributeByteMetadata();
    metadata.width = 2;
    metadata.height = 3;
    metadata.pixelColor = 1;
    metadata.pixelBit = pixelBit;
    return metadata;
}

}  // namespace

TEST(BmpImageFormat, BuildsLegacyAttributeHeaderBytes) {
    const auto header = Dss::Storage::buildLegacyBmpAttributeHeader(attributeByteMetadata());

    const std::array<std::uint8_t, Dss::Storage::kLegacyBmpAttributeSize> expected{
        0x42, 0x47, 0x59,        // BGY marker
        0x09, 0x01,              // legacy version bytes
        0x03, 0x02, 0x01,        // target id, 24-bit little endian
        0x34, 0x12,              // telescope id
        0x1A, 0x06, 0x02,        // 2026-06-02
        0x10, 0x27, 0x00, 0x00,  // one second in 1/10000 s units
        0x46, 0x00,              // ZT code
        0x0C, 0x00,              // delta X * 10
        0xDE, 0xFF,              // delta Y * 10
        0x0A, 0x0B,              // pixel scales
        0x00, 0x00, 0x40,        // 90 deg over 24-bit turn
        0x00, 0x00, 0x20,        // 45 deg over 24-bit turn
        0x0A, 0x00, 0x00, 0x00,  // distance * 4
        0x39, 0x30, 0x00,        // focal length * 100
        0xF6, 0x09,              // frame rate * 100
        0xB3, 0x00,              // temperature * 10
        0x1A, 0x00,              // humidity * 100
        0xD3, 0x1E,              // pressure / 100 * 10
        0x22, 0x00,              // wind speed * 10
        0x0C,                    // cloud cover * 10
        0x7B, 0x00, 0x00,        // exposure * 10
        0x00, 0x00, 0x00, 0x00,  // reserved tail
        0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(header, expected);
}

TEST(BmpImageFormat, DecodesLegacyAttributeHeader) {
    const auto header = Dss::Storage::buildLegacyBmpAttributeHeader(roundTripMetadata());

    const auto decoded = Dss::Storage::decodeLegacyBmpAttributeHeader(header);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->targetId, 0x010203U);
    EXPECT_EQ(decoded->telescopeId, 0x1234U);
    EXPECT_EQ(decoded->timestamp.year, 2026);
    EXPECT_EQ(decoded->timestamp.month, 6);
    EXPECT_EQ(decoded->timestamp.day, 2);
    EXPECT_EQ(decoded->timestamp.hour, 1);
    EXPECT_EQ(decoded->timestamp.minute, 2);
    EXPECT_EQ(decoded->timestamp.second, 3);
    EXPECT_EQ(decoded->timestamp.millisecond, 4);
    EXPECT_EQ(decoded->timestamp.microsecond, 400);
    EXPECT_EQ(decoded->ztCode, 0x0046U);
    EXPECT_DOUBLE_EQ(decoded->deltaX, 1.2);
    EXPECT_DOUBLE_EQ(decoded->deltaY, -3.4);
    EXPECT_EQ(decoded->pixelScaleX, 10U);
    EXPECT_EQ(decoded->pixelScaleY, 11U);
    EXPECT_DOUBLE_EQ(decoded->azimuthDegrees, 90.0);
    EXPECT_DOUBLE_EQ(decoded->elevationDegrees, 45.0);
    EXPECT_DOUBLE_EQ(decoded->distance, 2.5);
    EXPECT_DOUBLE_EQ(decoded->focalLength, 123.45);
    EXPECT_DOUBLE_EQ(decoded->frameRate, 25.5);
    EXPECT_DOUBLE_EQ(decoded->temperature, 17.9);
    EXPECT_DOUBLE_EQ(decoded->humidity, 0.26);
    EXPECT_DOUBLE_EQ(decoded->atmosPressure, 78910.0);
    EXPECT_DOUBLE_EQ(decoded->windSpeed, 3.4);
    EXPECT_DOUBLE_EQ(decoded->cloudCover, 1.2);
    EXPECT_DOUBLE_EQ(decoded->exposure, 12.3);
}

TEST(BmpImageFormat, RejectsInvalidAttributeHeader) {
    std::array<std::uint8_t, Dss::Storage::kLegacyBmpAttributeSize> badMagic{};
    badMagic[0] = 0x42;
    badMagic[1] = 0x4D;
    badMagic[2] = 0x50;

    const std::array<std::uint8_t, Dss::Storage::kLegacyBmpAttributeSize - 1> truncated{};

    EXPECT_FALSE(Dss::Storage::decodeLegacyBmpAttributeHeader(badMagic).has_value());
    EXPECT_FALSE(Dss::Storage::decodeLegacyBmpAttributeHeader(truncated).has_value());
}

TEST(BmpImageFormat, BuildsBitmapHeadersWithoutPaletteForSixteenBitImages) {
    const auto metadata = smallBmpMetadata(16);

    const auto fileHeader = Dss::Storage::buildLegacyBitmapFileHeader(metadata);
    const auto infoHeader = Dss::Storage::buildLegacyBitmapInfoHeader(metadata);

    EXPECT_EQ(fileHeader.type, 0x4D42U);
    EXPECT_EQ(fileHeader.offBits, 114U);
    EXPECT_EQ(fileHeader.size, 126U);
    EXPECT_EQ(infoHeader.size, 40U);
    EXPECT_EQ(infoHeader.width, 2U);
    EXPECT_EQ(infoHeader.height, 3U);
    EXPECT_EQ(infoHeader.planes, 1U);
    EXPECT_EQ(infoHeader.bitCount, 16U);
    EXPECT_EQ(infoHeader.compression, 0U);
    EXPECT_EQ(infoHeader.sizeImage, 12U);
    EXPECT_EQ(infoHeader.clrUsed, 0U);
    EXPECT_TRUE(Dss::Storage::buildLegacyRgbQuadPalette(16).empty());
}

TEST(BmpImageFormat, BuildsBitmapHeadersAndGrayPaletteForEightBitImages) {
    const auto metadata = smallBmpMetadata(8);

    const auto fileHeader = Dss::Storage::buildLegacyBitmapFileHeader(metadata);
    const auto infoHeader = Dss::Storage::buildLegacyBitmapInfoHeader(metadata);
    const auto palette = Dss::Storage::buildLegacyRgbQuadPalette(8);

    EXPECT_EQ(fileHeader.offBits, 1138U);
    EXPECT_EQ(fileHeader.size, 1144U);
    EXPECT_EQ(infoHeader.sizeImage, 6U);
    EXPECT_EQ(infoHeader.clrUsed, 256U);
    ASSERT_EQ(palette.size(), 256U);
    EXPECT_EQ(palette.front().blue, 0U);
    EXPECT_EQ(palette.front().green, 0U);
    EXPECT_EQ(palette.front().red, 0U);
    EXPECT_EQ(palette.front().reserved, 0U);
    EXPECT_EQ(palette.back().blue, 255U);
    EXPECT_EQ(palette.back().green, 255U);
    EXPECT_EQ(palette.back().red, 255U);
    EXPECT_EQ(palette.back().reserved, 0U);
}

TEST(BmpImageFormat, BuildsCompleteLegacyBmpFileInMemory) {
    const auto metadata = smallBmpMetadata(16);
    const std::vector<std::uint8_t> pixels{0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A,
                                           0xF0, 0xDE, 0x11, 0x22, 0x33, 0x44};

    const auto file = Dss::Storage::buildLegacyBmpFile(metadata, pixels);

    ASSERT_TRUE(file.has_value());
    ASSERT_EQ(file->size(), 126U);
    EXPECT_EQ((*file)[0], 0x42);
    EXPECT_EQ((*file)[1], 0x4D);
    EXPECT_EQ((*file)[10], 0x72);
    EXPECT_EQ((*file)[14], 0x28);
    EXPECT_EQ((*file)[54], 0x42);
    EXPECT_EQ((*file)[55], 0x47);
    EXPECT_EQ((*file)[56], 0x59);
    EXPECT_EQ((*file)[114], 0x34);
    EXPECT_EQ((*file)[125], 0x44);
}

TEST(BmpImageFormat, DecodesCompleteLegacyBmpFilePixels) {
    const auto metadata = smallBmpMetadata(16);
    const std::vector<std::uint8_t> pixels{0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A,
                                           0xF0, 0xDE, 0x11, 0x22, 0x33, 0x44};
    const auto file = Dss::Storage::buildLegacyBmpFile(metadata, pixels);
    ASSERT_TRUE(file.has_value());

    const auto decoded = Dss::Storage::decodeLegacyBmpFile(*file);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->metadata.width, 2U);
    EXPECT_EQ(decoded->metadata.height, 3U);
    EXPECT_EQ(decoded->metadata.pixelBit, 16U);
    EXPECT_EQ(decoded->metadata.pixelColor, 1U);
    EXPECT_EQ(decoded->fileHeader.offBits, 114U);
    EXPECT_EQ(decoded->infoHeader.sizeImage, 12U);
    const std::vector<std::uint16_t> expectedPixels{0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x2211, 0x4433};
    EXPECT_EQ(decoded->pixels, expectedPixels);
}

TEST(BmpImageFormat, RejectsPixelPayloadWithUnexpectedSize) {
    const auto metadata = smallBmpMetadata(16);
    const std::vector<std::uint8_t> shortPixels{0x34, 0x12};

    EXPECT_FALSE(Dss::Storage::buildLegacyBmpFile(metadata, shortPixels).has_value());
}
