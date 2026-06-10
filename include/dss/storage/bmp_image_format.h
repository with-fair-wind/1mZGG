#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kBitmapFileHeaderSize = 14U;       ///< BMP 文件头字节长度
inline constexpr auto kBitmapInfoHeaderSize = 40U;       ///< BMP 信息头字节长度
inline constexpr auto kLegacyBmpAttributeSize = 60U;     ///< 遗留 BMP 自定义属性块字节长度
inline constexpr auto kLegacyBmpPaletteEntries = 256U;    ///< 遗留 BMP 调色板条目数

/// 遗留 BMP 自定义属性元数据，嵌入文件头与像素数据之间
struct LegacyBmpMetadata {
    std::uint32_t targetId = 0;              ///< 目标编号
    std::uint16_t telescopeId = 0;           ///< 望远镜编号
    Dss::Core::Timestamp timestamp{};        ///< 采集时间戳
    std::uint16_t ztCode = 0;                ///< 状态/类型编码
    double deltaX = 0.0;                       ///< X 方向偏差
    double deltaY = 0.0;                       ///< Y 方向偏差
    std::uint8_t pixelScaleX = 0;            ///< X 方向像素尺度
    std::uint8_t pixelScaleY = 0;            ///< Y 方向像素尺度
    double azimuthDegrees = 0.0;             ///< 方位角（度）
    double elevationDegrees = 0.0;           ///< 俯仰角（度）
    double distance = 0.0;                     ///< 距离
    double focalLength = 0.0;                  ///< 焦距
    double frameRate = 0.0;                    ///< 帧率
    double temperature = 0.0;                  ///< 温度
    double humidity = 0.0;                     ///< 湿度
    double atmosPressure = 0.0;                ///< 大气压
    double windSpeed = 0.0;                    ///< 风速
    double cloudCover = 0.0;                   ///< 云量
    double exposure = 0.0;                     ///< 曝光量
    std::uint32_t width = 0;                 ///< 图像宽度（像素）
    std::uint32_t height = 0;                ///< 图像高度（像素）
    std::uint16_t pixelColor = 1;            ///< 像素通道数
    std::uint16_t pixelBit = 16;             ///< 每像素位深
};

/// 标准 BMP 文件头（BITMAPFILEHEADER）
struct BitmapFileHeader {
    std::uint16_t type = 0x4D42;  ///< 文件类型标识（"BM"）
    std::uint32_t size = 0;       ///< 文件总字节数
    std::uint16_t reserved1 = 0;  ///< 保留字段 1
    std::uint16_t reserved2 = 0;  ///< 保留字段 2
    std::uint32_t offBits = 0;    ///< 像素数据起始偏移
};

/// 标准 BMP 信息头（BITMAPINFOHEADER）
struct BitmapInfoHeader {
    std::uint32_t size = kBitmapInfoHeaderSize;  ///< 信息头结构体大小
    std::uint32_t width = 0;                     ///< 图像宽度（像素）
    std::uint32_t height = 0;                    ///< 图像高度（像素）
    std::uint16_t planes = 1;                    ///< 颜色平面数
    std::uint16_t bitCount = 0;                  ///< 每像素位数
    std::uint32_t compression = 0;               ///< 压缩方式
    std::uint32_t sizeImage = 0;                 ///< 像素数据字节数
    std::uint32_t xPelsPerMeter = 0;             ///< 水平分辨率（像素/米）
    std::uint32_t yPelsPerMeter = 0;             ///< 垂直分辨率（像素/米）
    std::uint32_t clrUsed = 0;                   ///< 实际使用的调色板颜色数
    std::uint32_t clrImportant = 0;              ///< 重要颜色数
};

/// BMP 调色板颜色项（RGBQUAD）
struct RgbQuad {
    std::uint8_t blue = 0;      ///< 蓝色分量
    std::uint8_t green = 0;     ///< 绿色分量
    std::uint8_t red = 0;       ///< 红色分量
    std::uint8_t reserved = 0;  ///< 保留字节
};

namespace Detail {

inline constexpr auto kAngleTurnUnits = 16777216.0;  ///< 遗留格式角度编码满量程单位数

/**
 * @brief 以小端序写入 16 位无符号整数
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
inline void writeLittleEndian16(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint16_t value) {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

/**
 * @brief 以小端序写入 24 位无符号整数
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
inline void writeLittleEndian24(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint32_t value) {
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
inline void writeLittleEndian32(std::span<std::uint8_t> output, std::size_t offset,
                                std::uint32_t value) {
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
inline auto readLittleEndian16(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint16_t {
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
inline auto readLittleEndian24(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
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
inline auto readLittleEndian32(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(input[offset + 3]) << 24U);
}

/**
 * @brief 将浮点值按指定比例缩放为无符号 32 位整数
 * @param value 原始浮点值
 * @param scale 缩放因子
 * @return 缩放后的整数值
 */
inline auto scaledUnsigned(double value, double scale) -> std::uint32_t {
    return static_cast<std::uint32_t>(value * scale);
}

/**
 * @brief 将浮点值按指定比例缩放为有符号 16 位整数（以 uint16 位模式存储）
 * @param value 原始浮点值
 * @param scale 缩放因子
 * @return 缩放后的 16 位值
 */
inline auto scaledSigned16(double value, double scale) -> std::uint16_t {
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
inline auto readScaledSigned16(std::span<const std::uint8_t> input, std::size_t offset,
                               double scale) -> double {
    const auto value = static_cast<std::int16_t>(readLittleEndian16(input, offset));
    return static_cast<double>(value) / scale;
}

/**
 * @brief 将时间戳编码为遗留格式的日内 tick 数（1 tick = 0.1 ms）
 * @param timestamp 源时间戳
 * @return 编码后的 tick 值
 */
inline auto timestampTicks(const Dss::Core::Timestamp& timestamp) -> std::uint32_t {
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

/**
 * @brief 将角度（度）编码为遗留 24 位格式
 * @param degrees 角度值（度）
 * @return 编码后的 24 位值
 */
inline auto angleToLegacy24(double degrees) -> std::uint32_t {
    return static_cast<std::uint32_t>(degrees / 360.0 * kAngleTurnUnits);
}

/**
 * @brief 从遗留 24 位格式解码角度（度）
 * @param value 编码值
 * @return 角度值（度）
 */
inline auto angleFromLegacy24(std::uint32_t value) -> double {
    return static_cast<double>(value) / kAngleTurnUnits * 360.0;
}

/**
 * @brief 计算遗留 BMP 调色板占用的字节数
 * @param pixelBit 每像素位深
 * @return 调色板字节数，位深大于 8 时返回 0
 */
inline auto legacyPaletteByteSize(std::uint16_t pixelBit) -> std::uint32_t {
    if (pixelBit <= 8U) {
        return kLegacyBmpPaletteEntries * sizeof(RgbQuad);
    }
    return 0;
}

/**
 * @brief 计算遗留 BMP 实际使用的调色板颜色数
 * @param pixelBit 每像素位深
 * @return 颜色数，位深大于 8 时返回 0
 */
inline auto legacyColorUsed(std::uint16_t pixelBit) -> std::uint32_t {
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

/**
 * @brief 将固定长度字节数组追加到输出向量
 * @param output 目标字节向量
 * @param bytes 待追加的字节数组
 */
template <typename T, std::size_t Size>
inline void appendArray(std::vector<std::uint8_t>& output, const std::array<T, Size>& bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

}  // namespace Detail

/**
 * @brief 构建遗留 BMP 自定义属性块（60 字节）
 * @param metadata 源元数据
 * @return 编码后的属性块字节数组
 */
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

/**
 * @brief 解码遗留 BMP 自定义属性块
 * @param header 属性块字节数据
 * @return 解码后的元数据；格式无效时返回空
 */
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

/**
 * @brief 计算遗留 BMP 像素数据的字节数
 * @param metadata 包含宽高与位深信息的元数据
 * @return 像素数据字节数；校验失败时返回空
 */
inline auto legacyBmpImageByteSize(const LegacyBmpMetadata& metadata)
    -> std::optional<std::uint32_t> {
    return Detail::checkedImageByteSize(metadata);
}

/**
 * @brief 根据元数据构建遗留 BMP 文件头
 * @param metadata 源元数据
 * @return 填充偏移与文件大小的文件头结构
 */
inline auto buildLegacyBitmapFileHeader(const LegacyBmpMetadata& metadata) -> BitmapFileHeader {
    BitmapFileHeader header{};
    const auto imageSize = Detail::checkedImageByteSize(metadata).value_or(0U);
    header.offBits = kBitmapFileHeaderSize + kBitmapInfoHeaderSize +
                     Detail::legacyPaletteByteSize(metadata.pixelBit) + kLegacyBmpAttributeSize;
    header.size = header.offBits + imageSize;
    return header;
}

/**
 * @brief 根据元数据构建遗留 BMP 信息头
 * @param metadata 源元数据
 * @return 填充尺寸与位深的信息头结构
 */
inline auto buildLegacyBitmapInfoHeader(const LegacyBmpMetadata& metadata) -> BitmapInfoHeader {
    BitmapInfoHeader header{};
    header.width = metadata.width;
    header.height = metadata.height;
    header.bitCount = metadata.pixelBit;
    header.sizeImage = Detail::checkedImageByteSize(metadata).value_or(0U);
    header.clrUsed = Detail::legacyColorUsed(metadata.pixelBit);
    return header;
}

/**
 * @brief 构建遗留 BMP 灰度调色板
 * @param pixelBit 每像素位深
 * @return 256 级灰度调色板；位深大于 8 时返回空向量
 */
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

/**
 * @brief 将 BMP 文件头结构编码为字节数组
 * @param header 源文件头
 * @return 14 字节编码结果
 */
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

/**
 * @brief 将 BMP 信息头结构编码为字节数组
 * @param header 源信息头
 * @return 40 字节编码结果
 */
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

/**
 * @brief 将 RGBQUAD 结构编码为 4 字节数组
 * @param quad 源颜色项
 * @return 4 字节编码结果
 */
inline auto encodeRgbQuad(const RgbQuad& quad) -> std::array<std::uint8_t, sizeof(RgbQuad)> {
    return {quad.blue, quad.green, quad.red, quad.reserved};
}

/**
 * @brief 组装完整的遗留 BMP 文件
 * @param metadata 图像元数据
 * @param imageData 原始像素数据
 * @return 完整 BMP 文件字节流；像素大小不匹配时返回空
 */
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
