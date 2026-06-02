#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kBitmapFileHeaderSize = 14U;
inline constexpr auto kBitmapInfoHeaderSize = 40U;
inline constexpr auto kLegacyBmpAttributeSize = 60U;
inline constexpr auto kLegacyBmpPaletteEntries = 256U;

struct LegacyBmpMetadata {
    std::uint32_t targetId = 0;
    std::uint16_t telescopeId = 0;
    Dss::Core::Timestamp timestamp{};
    std::uint16_t ztCode = 0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    std::uint8_t pixelScaleX = 0;
    std::uint8_t pixelScaleY = 0;
    double azimuthDegrees = 0.0;
    double elevationDegrees = 0.0;
    double distance = 0.0;
    double focalLength = 0.0;
    double frameRate = 0.0;
    double temperature = 0.0;
    double humidity = 0.0;
    double atmosPressure = 0.0;
    double windSpeed = 0.0;
    double cloudCover = 0.0;
    double exposure = 0.0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint16_t pixelColor = 1;
    std::uint16_t pixelBit = 16;
};

struct BitmapFileHeader {
    std::uint16_t type = 0x4D42;
    std::uint32_t size = 0;
    std::uint16_t reserved1 = 0;
    std::uint16_t reserved2 = 0;
    std::uint32_t offBits = 0;
};

struct BitmapInfoHeader {
    std::uint32_t size = kBitmapInfoHeaderSize;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint16_t planes = 1;
    std::uint16_t bitCount = 0;
    std::uint32_t compression = 0;
    std::uint32_t sizeImage = 0;
    std::uint32_t xPelsPerMeter = 0;
    std::uint32_t yPelsPerMeter = 0;
    std::uint32_t clrUsed = 0;
    std::uint32_t clrImportant = 0;
};

struct RgbQuad {
    std::uint8_t blue = 0;
    std::uint8_t green = 0;
    std::uint8_t red = 0;
    std::uint8_t reserved = 0;
};

namespace Detail {

inline constexpr auto kAngleTurnUnits = 16777216.0;

inline void writeLittleEndian16(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint16_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

inline void writeLittleEndian24(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
}

inline void writeLittleEndian32(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    output[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
}

inline auto readLittleEndian16(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint16_t {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(input[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(input[offset + 1]) << 8U));
}

inline auto readLittleEndian24(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16U);
}

inline auto readLittleEndian32(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(input[offset + 3]) << 24U);
}

inline auto scaledUnsigned(double value, double scale) -> std::uint32_t {
    return static_cast<std::uint32_t>(value * scale);
}

inline auto scaledSigned16(double value, double scale) -> std::uint16_t {
    const auto scaled = static_cast<std::int16_t>(value * scale);
    return static_cast<std::uint16_t>(scaled);
}

inline auto readScaledSigned16(std::span<const std::uint8_t> input, std::size_t offset,
                               double scale) -> double {
    const auto value = static_cast<std::int16_t>(readLittleEndian16(input, offset));
    return static_cast<double>(value) / scale;
}

inline auto timestampTicks(const Dss::Core::Timestamp& timestamp) -> std::uint32_t {
    const auto seconds = timestamp.hour * 3600 + timestamp.minute * 60 + timestamp.second;
    const auto totalSeconds = static_cast<double>(seconds) +
                              static_cast<double>(timestamp.millisecond) * 0.001 +
                              static_cast<double>(timestamp.microsecond) * 0.000001;
    return static_cast<std::uint32_t>(totalSeconds * 10000.0);
}

inline auto timestampFromTicks(std::uint32_t ticks) -> Dss::Core::Timestamp {
    Dss::Core::Timestamp timestamp{};
    const auto wholeSeconds = ticks / 10000U;
    const auto fractionalTicks = ticks % 10000U;
    timestamp.hour = static_cast<int>(wholeSeconds / 3600U);
    timestamp.minute = static_cast<int>((wholeSeconds % 3600U) / 60U);
    timestamp.second = static_cast<int>(wholeSeconds % 60U);
    timestamp.millisecond = static_cast<int>(fractionalTicks / 10U);
    timestamp.microsecond = static_cast<int>((fractionalTicks % 10U) * 100U);
    return timestamp;
}

inline auto angleToLegacy24(double degrees) -> std::uint32_t {
    return static_cast<std::uint32_t>(degrees / 360.0 * kAngleTurnUnits);
}

inline auto angleFromLegacy24(std::uint32_t value) -> double {
    return static_cast<double>(value) / kAngleTurnUnits * 360.0;
}

inline auto legacyPaletteByteSize(std::uint16_t pixelBit) -> std::uint32_t {
    if (pixelBit <= 8U) {
        return kLegacyBmpPaletteEntries * sizeof(RgbQuad);
    }
    return 0;
}

inline auto legacyColorUsed(std::uint16_t pixelBit) -> std::uint32_t {
    if (pixelBit <= 8U) {
        return 1U << pixelBit;
    }
    return 0;
}

inline auto checkedImageByteSize(const LegacyBmpMetadata& metadata)
    -> std::optional<std::uint32_t> {
    const auto bits = static_cast<std::uint64_t>(metadata.width) *
                      static_cast<std::uint64_t>(metadata.height) *
                      static_cast<std::uint64_t>(metadata.pixelColor) *
                      static_cast<std::uint64_t>(metadata.pixelBit);
    const auto bytes = bits / 8U;
    if (bits % 8U != 0U || bytes > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(bytes);
}

template <typename T, std::size_t Size>
inline void appendArray(std::vector<std::uint8_t>& output, const std::array<T, Size>& bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

}  // namespace Detail

inline auto buildLegacyBmpAttributeHeader(const LegacyBmpMetadata& metadata)
    -> std::array<std::uint8_t, kLegacyBmpAttributeSize> {
    std::array<std::uint8_t, kLegacyBmpAttributeSize> header{};
    header[0] = 0x42;
    header[1] = 0x47;
    header[2] = 0x59;
    header[3] = 0x09;
    header[4] = 0x01;
    Detail::writeLittleEndian24(header, 5, metadata.targetId);
    Detail::writeLittleEndian16(header, 8, metadata.telescopeId);
    header[10] = static_cast<std::uint8_t>(metadata.timestamp.year - 2000);
    header[11] = static_cast<std::uint8_t>(metadata.timestamp.month);
    header[12] = static_cast<std::uint8_t>(metadata.timestamp.day);
    Detail::writeLittleEndian32(header, 13, Detail::timestampTicks(metadata.timestamp));
    Detail::writeLittleEndian16(header, 17, metadata.ztCode);
    Detail::writeLittleEndian16(header, 19, Detail::scaledSigned16(metadata.deltaX, 10.0));
    Detail::writeLittleEndian16(header, 21, Detail::scaledSigned16(metadata.deltaY, 10.0));
    header[23] = metadata.pixelScaleX;
    header[24] = metadata.pixelScaleY;
    Detail::writeLittleEndian24(header, 25, Detail::angleToLegacy24(metadata.azimuthDegrees));
    Detail::writeLittleEndian24(header, 28, Detail::angleToLegacy24(metadata.elevationDegrees));
    Detail::writeLittleEndian32(header, 31, Detail::scaledUnsigned(metadata.distance, 4.0));
    Detail::writeLittleEndian24(header, 35, Detail::scaledUnsigned(metadata.focalLength, 100.0));
    Detail::writeLittleEndian16(
        header, 38, static_cast<std::uint16_t>(Detail::scaledUnsigned(metadata.frameRate, 100.0)));
    Detail::writeLittleEndian16(
        header, 40, static_cast<std::uint16_t>(Detail::scaledUnsigned(metadata.temperature, 10.0)));
    Detail::writeLittleEndian16(
        header, 42, static_cast<std::uint16_t>(Detail::scaledUnsigned(metadata.humidity, 100.0)));
    Detail::writeLittleEndian16(
        header, 44,
        static_cast<std::uint16_t>(Detail::scaledUnsigned(metadata.atmosPressure / 100.0, 10.0)));
    Detail::writeLittleEndian16(
        header, 46, static_cast<std::uint16_t>(Detail::scaledUnsigned(metadata.windSpeed, 10.0)));
    header[48] = static_cast<std::uint8_t>(Detail::scaledUnsigned(metadata.cloudCover, 10.0));
    Detail::writeLittleEndian24(header, 49, Detail::scaledUnsigned(metadata.exposure, 10.0));
    return header;
}

inline auto decodeLegacyBmpAttributeHeader(std::span<const std::uint8_t> header)
    -> std::optional<LegacyBmpMetadata> {
    if (header.size() < kLegacyBmpAttributeSize || header[0] != 0x42U || header[1] != 0x47U ||
        header[2] != 0x59U) {
        return std::nullopt;
    }

    LegacyBmpMetadata metadata{};
    metadata.targetId = Detail::readLittleEndian24(header, 5);
    metadata.telescopeId = Detail::readLittleEndian16(header, 8);
    metadata.timestamp.year = 2000 + static_cast<int>(header[10]);
    metadata.timestamp.month = static_cast<int>(header[11]);
    metadata.timestamp.day = static_cast<int>(header[12]);
    const auto time = Detail::timestampFromTicks(Detail::readLittleEndian32(header, 13));
    metadata.timestamp.hour = time.hour;
    metadata.timestamp.minute = time.minute;
    metadata.timestamp.second = time.second;
    metadata.timestamp.millisecond = time.millisecond;
    metadata.timestamp.microsecond = time.microsecond;
    metadata.ztCode = Detail::readLittleEndian16(header, 17);
    metadata.deltaX = Detail::readScaledSigned16(header, 19, 10.0);
    metadata.deltaY = Detail::readScaledSigned16(header, 21, 10.0);
    metadata.pixelScaleX = header[23];
    metadata.pixelScaleY = header[24];
    metadata.azimuthDegrees = Detail::angleFromLegacy24(Detail::readLittleEndian24(header, 25));
    metadata.elevationDegrees = Detail::angleFromLegacy24(Detail::readLittleEndian24(header, 28));
    metadata.distance = Detail::readLittleEndian32(header, 31) / 4.0;
    metadata.focalLength = Detail::readLittleEndian24(header, 35) / 100.0;
    metadata.frameRate = Detail::readLittleEndian16(header, 38) / 100.0;
    metadata.temperature = Detail::readLittleEndian16(header, 40) / 10.0;
    metadata.humidity = Detail::readLittleEndian16(header, 42) / 100.0;
    metadata.atmosPressure = Detail::readLittleEndian16(header, 44) / 10.0 * 100.0;
    metadata.windSpeed = Detail::readLittleEndian16(header, 46) / 10.0;
    metadata.cloudCover = header[48] / 10.0;
    metadata.exposure = Detail::readLittleEndian24(header, 49) / 10.0;
    return metadata;
}

inline auto legacyBmpImageByteSize(const LegacyBmpMetadata& metadata)
    -> std::optional<std::uint32_t> {
    return Detail::checkedImageByteSize(metadata);
}

inline auto buildLegacyBitmapFileHeader(const LegacyBmpMetadata& metadata) -> BitmapFileHeader {
    BitmapFileHeader header{};
    const auto imageSize = Detail::checkedImageByteSize(metadata).value_or(0U);
    header.offBits = kBitmapFileHeaderSize + kBitmapInfoHeaderSize +
                     Detail::legacyPaletteByteSize(metadata.pixelBit) + kLegacyBmpAttributeSize;
    header.size = header.offBits + imageSize;
    return header;
}

inline auto buildLegacyBitmapInfoHeader(const LegacyBmpMetadata& metadata) -> BitmapInfoHeader {
    BitmapInfoHeader header{};
    header.width = metadata.width;
    header.height = metadata.height;
    header.bitCount = metadata.pixelBit;
    header.sizeImage = Detail::checkedImageByteSize(metadata).value_or(0U);
    header.clrUsed = Detail::legacyColorUsed(metadata.pixelBit);
    return header;
}

inline auto buildLegacyRgbQuadPalette(std::uint16_t pixelBit) -> std::vector<RgbQuad> {
    if (pixelBit > 8U) {
        return {};
    }

    std::vector<RgbQuad> palette;
    palette.reserve(kLegacyBmpPaletteEntries);
    for (std::uint16_t value = 0; value < kLegacyBmpPaletteEntries; ++value) {
        const auto level = static_cast<std::uint8_t>(value);
        palette.push_back({.blue = level, .green = level, .red = level, .reserved = 0});
    }
    return palette;
}

inline auto encodeBitmapFileHeader(const BitmapFileHeader& header)
    -> std::array<std::uint8_t, kBitmapFileHeaderSize> {
    std::array<std::uint8_t, kBitmapFileHeaderSize> bytes{};
    Detail::writeLittleEndian16(bytes, 0, header.type);
    Detail::writeLittleEndian32(bytes, 2, header.size);
    Detail::writeLittleEndian16(bytes, 6, header.reserved1);
    Detail::writeLittleEndian16(bytes, 8, header.reserved2);
    Detail::writeLittleEndian32(bytes, 10, header.offBits);
    return bytes;
}

inline auto encodeBitmapInfoHeader(const BitmapInfoHeader& header)
    -> std::array<std::uint8_t, kBitmapInfoHeaderSize> {
    std::array<std::uint8_t, kBitmapInfoHeaderSize> bytes{};
    Detail::writeLittleEndian32(bytes, 0, header.size);
    Detail::writeLittleEndian32(bytes, 4, header.width);
    Detail::writeLittleEndian32(bytes, 8, header.height);
    Detail::writeLittleEndian16(bytes, 12, header.planes);
    Detail::writeLittleEndian16(bytes, 14, header.bitCount);
    Detail::writeLittleEndian32(bytes, 16, header.compression);
    Detail::writeLittleEndian32(bytes, 20, header.sizeImage);
    Detail::writeLittleEndian32(bytes, 24, header.xPelsPerMeter);
    Detail::writeLittleEndian32(bytes, 28, header.yPelsPerMeter);
    Detail::writeLittleEndian32(bytes, 32, header.clrUsed);
    Detail::writeLittleEndian32(bytes, 36, header.clrImportant);
    return bytes;
}

inline auto encodeRgbQuad(const RgbQuad& quad) -> std::array<std::uint8_t, sizeof(RgbQuad)> {
    return {quad.blue, quad.green, quad.red, quad.reserved};
}

inline auto buildLegacyBmpFile(const LegacyBmpMetadata& metadata,
                               std::span<const std::uint8_t> imageData)
    -> std::optional<std::vector<std::uint8_t>> {
    const auto imageSize = Detail::checkedImageByteSize(metadata);
    if (!imageSize.has_value() || imageData.size() != *imageSize) {
        return std::nullopt;
    }

    const auto fileHeader = buildLegacyBitmapFileHeader(metadata);
    const auto infoHeader = buildLegacyBitmapInfoHeader(metadata);
    const auto attributeHeader = buildLegacyBmpAttributeHeader(metadata);
    const auto palette = buildLegacyRgbQuadPalette(metadata.pixelBit);

    std::vector<std::uint8_t> file;
    file.reserve(fileHeader.size);
    Detail::appendArray(file, encodeBitmapFileHeader(fileHeader));
    Detail::appendArray(file, encodeBitmapInfoHeader(infoHeader));
    for (const auto& color : palette) {
        Detail::appendArray(file, encodeRgbQuad(color));
    }
    Detail::appendArray(file, attributeHeader);
    file.insert(file.end(), imageData.begin(), imageData.end());
    return file;
}

}  // namespace Dss::Storage
