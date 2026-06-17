#include "dss/storage/bmp_image_format.h"

#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>

namespace Dss::Storage {
namespace {

inline constexpr auto kAngleTurnUnits = 16777216.0;  ///< 遗留格式角度编码满量程单位数
inline constexpr auto kRgbQuadSize = 4U;             ///< BMP 调色板单项字节数

/**
 * @brief 以小端序写入 16 位无符号整数
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
void writeLittleEndian16(std::span<std::uint8_t> output, std::size_t offset, std::uint16_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

/**
 * @brief 以小端序写入 24 位无符号整数
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
void writeLittleEndian24(std::span<std::uint8_t> output, std::size_t offset, std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
}

/**
 * @brief 以小端序写入 32 位无符号整数
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
void writeLittleEndian32(std::span<std::uint8_t> output, std::size_t offset, std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    output[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
}

/**
 * @brief 从小端序字节流读取 16 位无符号整数
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @return 解码后的 16 位值
 */
auto readLittleEndian16(std::span<const std::uint8_t> input, std::size_t offset) -> std::uint16_t {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(input[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(input[offset + 1]) << 8U));
}

/**
 * @brief 从小端序字节流读取 24 位无符号整数
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @return 解码后的 24 位值
 */
auto readLittleEndian24(std::span<const std::uint8_t> input, std::size_t offset) -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16U);
}

/**
 * @brief 从小端序字节流读取 32 位无符号整数
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @return 解码后的 32 位值
 */
auto readLittleEndian32(std::span<const std::uint8_t> input, std::size_t offset) -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(input[offset + 3]) << 24U);
}

/**
 * @brief 将 BMP 高度字段归一化为正数高度
 * @param value BMP 信息头中的原始高度字段
 * @return 正数图像高度
 */
auto normalizedBitmapHeight(std::uint32_t value) -> std::uint32_t {
    const auto signedValue = static_cast<std::int32_t>(value);
    if (signedValue < 0) {
        return static_cast<std::uint32_t>(-static_cast<std::int64_t>(signedValue));
    }
    return value;
}

/**
 * @brief 将浮点值按指定比例缩放为无符号 32 位整数
 * @param value 原始浮点值
 * @param scale 缩放因子
 * @return 缩放后的整数值
 */
auto scaledUnsigned(double value, double scale) -> std::uint32_t {
    return static_cast<std::uint32_t>(value * scale);
}

/**
 * @brief 将浮点值按指定比例缩放为有符号 16 位整数（以 uint16 位模式存储）
 * @param value 原始浮点值
 * @param scale 缩放因子
 * @return 缩放后的 16 位值
 */
auto scaledSigned16(double value, double scale) -> std::uint16_t {
    const auto scaled = static_cast<std::int16_t>(value * scale);
    return static_cast<std::uint16_t>(scaled);
}

/**
 * @brief 从字节流读取有符号 16 位缩放值并还原为浮点数
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @param scale 缩放因子
 * @return 还原后的浮点值
 */
auto readScaledSigned16(std::span<const std::uint8_t> input, std::size_t offset, double scale)
    -> double {
    const auto value = static_cast<std::int16_t>(readLittleEndian16(input, offset));
    return static_cast<double>(value) / scale;
}

/**
 * @brief 将时间戳编码为遗留格式的日内 tick 数（1 tick = 0.1 ms）
 * @param timestamp 源时间戳
 * @return 编码后的 tick 值
 */
auto timestampTicks(const Dss::Core::Timestamp& timestamp) -> std::uint32_t {
    const auto seconds = timestamp.hour * 3600 + timestamp.minute * 60 + timestamp.second;
    const auto totalSeconds = static_cast<double>(seconds) +
                              static_cast<double>(timestamp.millisecond) * 0.001 +
                              static_cast<double>(timestamp.microsecond) * 0.000001;
    return static_cast<std::uint32_t>(totalSeconds * 10000.0);
}

/**
 * @brief 从遗留 tick 数解码为时间戳的时分秒及亚秒字段
 * @param ticks 遗留格式 tick 值
 * @return 解码后的时间戳（仅时分秒及亚秒字段有效）
 */
auto timestampFromTicks(std::uint32_t ticks) -> Dss::Core::Timestamp {
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

/**
 * @brief 将角度（度）编码为遗留 24 位格式
 * @param degrees 角度值（度）
 * @return 编码后的 24 位值
 */
auto angleToLegacy24(double degrees) -> std::uint32_t {
    return static_cast<std::uint32_t>(degrees / 360.0 * kAngleTurnUnits);
}

/**
 * @brief 从遗留 24 位格式解码角度（度）
 * @param value 编码值
 * @return 角度值（度）
 */
auto angleFromLegacy24(std::uint32_t value) -> double {
    return static_cast<double>(value) / kAngleTurnUnits * 360.0;
}

/**
 * @brief 计算遗留 BMP 调色板占用的字节数
 * @param pixelBit 每像素位深
 * @return 调色板字节数，位深大于 8 时返回 0
 */
auto legacyPaletteByteSize(std::uint16_t pixelBit) -> std::uint32_t {
    if (pixelBit <= 8U) {
        return kLegacyBmpPaletteEntries * kRgbQuadSize;
    }
    return 0;
}

/**
 * @brief 计算遗留 BMP 实际使用的调色板颜色数
 * @param pixelBit 每像素位深
 * @return 颜色数，位深大于 8 时返回 0
 */
auto legacyColorUsed(std::uint16_t pixelBit) -> std::uint32_t {
    if (pixelBit <= 8U) {
        return 1U << pixelBit;
    }
    return 0;
}

/**
 * @brief 校验并计算像素数据的字节数
 * @param metadata 包含宽高与位深信息的元数据
 * @return 像素数据字节数；溢出或非整字节对齐时返回空
 */
auto checkedImageByteSize(const LegacyBmpMetadata& metadata) -> std::optional<std::uint32_t> {
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

/**
 * @brief 将固定长度字节数组追加到输出向量
 * @param output 目标字节向量
 * @param bytes 待追加的字节数组
 */
template <typename T, std::size_t Size>
void appendArray(std::vector<std::uint8_t>& output, const std::array<T, Size>& bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

/**
 * @brief 从字节流解码标准 BMP 文件头
 * @param bytes 文件头起始字节数据
 * @return 解码后的文件头；长度不足或魔数错误时返回空
 */
auto decodeBitmapFileHeader(std::span<const std::uint8_t> bytes)
    -> std::optional<BitmapFileHeader> {
    if (bytes.size() < kBitmapFileHeaderSize) {
        return std::nullopt;
    }

    BitmapFileHeader header{};
    header.type = readLittleEndian16(bytes, 0);
    if (header.type != 0x4D42U) {
        return std::nullopt;
    }
    header.size = readLittleEndian32(bytes, 2);
    header.reserved1 = readLittleEndian16(bytes, 6);
    header.reserved2 = readLittleEndian16(bytes, 8);
    header.offBits = readLittleEndian32(bytes, 10);
    return header;
}

/**
 * @brief 从字节流解码标准 BMP 信息头
 * @param bytes 信息头起始字节数据
 * @return 解码后的信息头；长度不足或信息头尺寸不匹配时返回空
 */
auto decodeBitmapInfoHeader(std::span<const std::uint8_t> bytes)
    -> std::optional<BitmapInfoHeader> {
    if (bytes.size() < kBitmapInfoHeaderSize) {
        return std::nullopt;
    }

    BitmapInfoHeader header{};
    header.size = readLittleEndian32(bytes, 0);
    if (header.size != kBitmapInfoHeaderSize) {
        return std::nullopt;
    }
    header.width = readLittleEndian32(bytes, 4);
    header.height = normalizedBitmapHeight(readLittleEndian32(bytes, 8));
    header.planes = readLittleEndian16(bytes, 12);
    header.bitCount = readLittleEndian16(bytes, 14);
    header.compression = readLittleEndian32(bytes, 16);
    header.sizeImage = readLittleEndian32(bytes, 20);
    header.xPelsPerMeter = readLittleEndian32(bytes, 24);
    header.yPelsPerMeter = readLittleEndian32(bytes, 28);
    header.clrUsed = readLittleEndian32(bytes, 32);
    header.clrImportant = readLittleEndian32(bytes, 36);
    return header;
}

/**
 * @brief 将 BMP 文件头结构编码为字节数组
 * @param header 源文件头
 * @return 14 字节编码结果
 */
auto encodeBitmapFileHeader(const BitmapFileHeader& header)
    -> std::array<std::uint8_t, kBitmapFileHeaderSize> {
    std::array<std::uint8_t, kBitmapFileHeaderSize> bytes{};
    writeLittleEndian16(bytes, 0, header.type);
    writeLittleEndian32(bytes, 2, header.size);
    writeLittleEndian16(bytes, 6, header.reserved1);
    writeLittleEndian16(bytes, 8, header.reserved2);
    writeLittleEndian32(bytes, 10, header.offBits);
    return bytes;
}

/**
 * @brief 将 BMP 信息头结构编码为字节数组
 * @param header 源信息头
 * @return 40 字节编码结果
 */
auto encodeBitmapInfoHeader(const BitmapInfoHeader& header)
    -> std::array<std::uint8_t, kBitmapInfoHeaderSize> {
    std::array<std::uint8_t, kBitmapInfoHeaderSize> bytes{};
    writeLittleEndian32(bytes, 0, header.size);
    writeLittleEndian32(bytes, 4, header.width);
    writeLittleEndian32(bytes, 8, header.height);
    writeLittleEndian16(bytes, 12, header.planes);
    writeLittleEndian16(bytes, 14, header.bitCount);
    writeLittleEndian32(bytes, 16, header.compression);
    writeLittleEndian32(bytes, 20, header.sizeImage);
    writeLittleEndian32(bytes, 24, header.xPelsPerMeter);
    writeLittleEndian32(bytes, 28, header.yPelsPerMeter);
    writeLittleEndian32(bytes, 32, header.clrUsed);
    writeLittleEndian32(bytes, 36, header.clrImportant);
    return bytes;
}

/**
 * @brief 将 RGBQUAD 结构编码为 4 字节数组
 * @param quad 源颜色项
 * @return 4 字节编码结果
 */
auto encodeRgbQuad(const RgbQuad& quad) -> std::array<std::uint8_t, kRgbQuadSize> {
    return {quad.blue, quad.green, quad.red, quad.reserved};
}

}  // namespace

auto buildLegacyBmpAttributeHeader(const LegacyBmpMetadata& metadata)
    -> std::array<std::uint8_t, kLegacyBmpAttributeSize> {
    std::array<std::uint8_t, kLegacyBmpAttributeSize> header{};
    header[0] = 0x42;
    header[1] = 0x47;
    header[2] = 0x59;
    header[3] = 0x09;
    header[4] = 0x01;
    writeLittleEndian24(header, 5, metadata.targetId);
    writeLittleEndian16(header, 8, metadata.telescopeId);
    header[10] = static_cast<std::uint8_t>(metadata.timestamp.year - 2000);
    header[11] = static_cast<std::uint8_t>(metadata.timestamp.month);
    header[12] = static_cast<std::uint8_t>(metadata.timestamp.day);
    writeLittleEndian32(header, 13, timestampTicks(metadata.timestamp));
    writeLittleEndian16(header, 17, metadata.ztCode);
    writeLittleEndian16(header, 19, scaledSigned16(metadata.deltaX, 10.0));
    writeLittleEndian16(header, 21, scaledSigned16(metadata.deltaY, 10.0));
    header[23] = metadata.pixelScaleX;
    header[24] = metadata.pixelScaleY;
    writeLittleEndian24(header, 25, angleToLegacy24(metadata.azimuthDegrees));
    writeLittleEndian24(header, 28, angleToLegacy24(metadata.elevationDegrees));
    writeLittleEndian32(header, 31, scaledUnsigned(metadata.distance, 4.0));
    writeLittleEndian24(header, 35, scaledUnsigned(metadata.focalLength, 100.0));
    writeLittleEndian16(header, 38,
                        static_cast<std::uint16_t>(scaledUnsigned(metadata.frameRate, 100.0)));
    writeLittleEndian16(header, 40,
                        static_cast<std::uint16_t>(scaledUnsigned(metadata.temperature, 10.0)));
    writeLittleEndian16(header, 42,
                        static_cast<std::uint16_t>(scaledUnsigned(metadata.humidity, 100.0)));
    writeLittleEndian16(
        header, 44,
        static_cast<std::uint16_t>(scaledUnsigned(metadata.atmosPressure / 100.0, 10.0)));
    writeLittleEndian16(header, 46,
                        static_cast<std::uint16_t>(scaledUnsigned(metadata.windSpeed, 10.0)));
    header[48] = static_cast<std::uint8_t>(scaledUnsigned(metadata.cloudCover, 10.0));
    writeLittleEndian24(header, 49, scaledUnsigned(metadata.exposure, 10.0));
    return header;
}

auto decodeLegacyBmpAttributeHeader(std::span<const std::uint8_t> header)
    -> std::optional<LegacyBmpMetadata> {
    if (header.size() < kLegacyBmpAttributeSize || header[0] != 0x42U || header[1] != 0x47U ||
        header[2] != 0x59U) {
        return std::nullopt;
    }

    LegacyBmpMetadata metadata{};
    metadata.targetId = readLittleEndian24(header, 5);
    metadata.telescopeId = readLittleEndian16(header, 8);
    metadata.timestamp.year = 2000 + static_cast<int>(header[10]);
    metadata.timestamp.month = static_cast<int>(header[11]);
    metadata.timestamp.day = static_cast<int>(header[12]);
    const auto time = timestampFromTicks(readLittleEndian32(header, 13));
    metadata.timestamp.hour = time.hour;
    metadata.timestamp.minute = time.minute;
    metadata.timestamp.second = time.second;
    metadata.timestamp.millisecond = time.millisecond;
    metadata.timestamp.microsecond = time.microsecond;
    metadata.ztCode = readLittleEndian16(header, 17);
    metadata.deltaX = readScaledSigned16(header, 19, 10.0);
    metadata.deltaY = readScaledSigned16(header, 21, 10.0);
    metadata.pixelScaleX = header[23];
    metadata.pixelScaleY = header[24];
    metadata.azimuthDegrees = angleFromLegacy24(readLittleEndian24(header, 25));
    metadata.elevationDegrees = angleFromLegacy24(readLittleEndian24(header, 28));
    metadata.distance = readLittleEndian32(header, 31) / 4.0;
    metadata.focalLength = readLittleEndian24(header, 35) / 100.0;
    metadata.frameRate = readLittleEndian16(header, 38) / 100.0;
    metadata.temperature = readLittleEndian16(header, 40) / 10.0;
    metadata.humidity = readLittleEndian16(header, 42) / 100.0;
    metadata.atmosPressure = readLittleEndian16(header, 44) / 10.0 * 100.0;
    metadata.windSpeed = readLittleEndian16(header, 46) / 10.0;
    metadata.cloudCover = header[48] / 10.0;
    metadata.exposure = readLittleEndian24(header, 49) / 10.0;
    return metadata;
}

auto legacyBmpImageByteSize(const LegacyBmpMetadata& metadata) -> std::optional<std::uint32_t> {
    return checkedImageByteSize(metadata);
}

auto buildLegacyBitmapFileHeader(const LegacyBmpMetadata& metadata) -> BitmapFileHeader {
    BitmapFileHeader header{};
    const auto imageSize = checkedImageByteSize(metadata).value_or(0U);
    header.offBits = kBitmapFileHeaderSize + kBitmapInfoHeaderSize +
                     legacyPaletteByteSize(metadata.pixelBit) + kLegacyBmpAttributeSize;
    header.size = header.offBits + imageSize;
    return header;
}

auto buildLegacyBitmapInfoHeader(const LegacyBmpMetadata& metadata) -> BitmapInfoHeader {
    BitmapInfoHeader header{};
    header.width = metadata.width;
    header.height = metadata.height;
    header.bitCount = metadata.pixelBit;
    header.sizeImage = checkedImageByteSize(metadata).value_or(0U);
    header.clrUsed = legacyColorUsed(metadata.pixelBit);
    return header;
}

auto buildLegacyRgbQuadPalette(std::uint16_t pixelBit) -> std::vector<RgbQuad> {
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

auto buildLegacyBmpFile(const LegacyBmpMetadata& metadata, std::span<const std::uint8_t> imageData)
    -> std::optional<std::vector<std::uint8_t>> {
    const auto imageSize = checkedImageByteSize(metadata);
    if (!imageSize.has_value() || imageData.size() != *imageSize) {
        return std::nullopt;
    }

    const auto fileHeader = buildLegacyBitmapFileHeader(metadata);
    const auto infoHeader = buildLegacyBitmapInfoHeader(metadata);
    const auto attributeHeader = buildLegacyBmpAttributeHeader(metadata);
    const auto palette = buildLegacyRgbQuadPalette(metadata.pixelBit);

    std::vector<std::uint8_t> file;
    file.reserve(fileHeader.size);
    appendArray(file, encodeBitmapFileHeader(fileHeader));
    appendArray(file, encodeBitmapInfoHeader(infoHeader));
    for (const auto& color : palette) {
        appendArray(file, encodeRgbQuad(color));
    }
    appendArray(file, attributeHeader);
    file.insert(file.end(), imageData.begin(), imageData.end());
    return file;
}

auto decodeLegacyBmpFile(std::span<const std::uint8_t> file) -> std::optional<LegacyBmpFile> {
    const auto fileHeader = decodeBitmapFileHeader(file);
    if (!fileHeader.has_value()) {
        return std::nullopt;
    }
    if (fileHeader->size != 0U && fileHeader->size > file.size()) {
        return std::nullopt;
    }

    const auto infoOffset = static_cast<std::size_t>(kBitmapFileHeaderSize);
    const auto infoHeader = decodeBitmapInfoHeader(file.subspan(infoOffset));
    if (!infoHeader.has_value()) {
        return std::nullopt;
    }
    if (infoHeader->planes != 1U || infoHeader->compression != 0U || infoHeader->width == 0U ||
        infoHeader->height == 0U || (infoHeader->bitCount != 8U && infoHeader->bitCount != 16U)) {
        return std::nullopt;
    }

    const auto paletteBytes = legacyPaletteByteSize(infoHeader->bitCount);
    const auto attributeOffset = kBitmapFileHeaderSize + kBitmapInfoHeaderSize + paletteBytes;
    const auto pixelOffset = static_cast<std::size_t>(fileHeader->offBits);
    if (file.size() < attributeOffset + kLegacyBmpAttributeSize ||
        pixelOffset < attributeOffset + kLegacyBmpAttributeSize || file.size() < pixelOffset) {
        return std::nullopt;
    }

    auto metadata =
        decodeLegacyBmpAttributeHeader(file.subspan(attributeOffset, kLegacyBmpAttributeSize));
    if (!metadata.has_value()) {
        return std::nullopt;
    }
    metadata->width = infoHeader->width;
    metadata->height = infoHeader->height;
    metadata->pixelColor = 1;
    metadata->pixelBit = infoHeader->bitCount;

    const auto payloadBytes = legacyBmpImageByteSize(*metadata);
    if (!payloadBytes.has_value() || file.size() < pixelOffset + *payloadBytes) {
        return std::nullopt;
    }
    if (infoHeader->sizeImage != 0U && infoHeader->sizeImage < *payloadBytes) {
        return std::nullopt;
    }

    const auto pixelBytes = file.subspan(pixelOffset, *payloadBytes);
    const auto pixelCount =
        static_cast<std::size_t>(metadata->width) * static_cast<std::size_t>(metadata->height);
    LegacyBmpFile decoded{};
    decoded.metadata = *metadata;
    decoded.fileHeader = *fileHeader;
    decoded.infoHeader = *infoHeader;
    decoded.pixels.resize(pixelCount);

    if (infoHeader->bitCount == 16U) {
        if (pixelBytes.size() != pixelCount * sizeof(std::uint16_t)) {
            return std::nullopt;
        }
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(decoded.pixels.data(), pixelBytes.data(), pixelBytes.size());
        } else {
            for (std::size_t index = 0; index < pixelCount; ++index) {
                decoded.pixels[index] =
                    readLittleEndian16(pixelBytes, index * sizeof(std::uint16_t));
            }
        }
    } else {
        if (pixelBytes.size() != pixelCount) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < pixelCount; ++index) {
            decoded.pixels[index] = static_cast<std::uint16_t>(pixelBytes[index]) << 8U;
        }
    }
    return decoded;
}

}  // namespace Dss::Storage
