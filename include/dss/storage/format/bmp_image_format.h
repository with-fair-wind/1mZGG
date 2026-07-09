#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kBitmapFileHeaderSize = 14U;      ///< BMP 文件头字节长度
inline constexpr auto kBitmapInfoHeaderSize = 40U;      ///< BMP 信息头字节长度
inline constexpr auto kLegacyBmpAttributeSize = 60U;    ///< 遗留 BMP 自定义属性块字节长度
inline constexpr auto kLegacyBmpPaletteEntries = 256U;  ///< 遗留 BMP 调色板条目数

/// 遗留 BMP 自定义属性元数据，嵌入文件头与像素数据之间
struct LegacyBmpMetadata {
    std::uint32_t targetId = 0;        ///< 目标编号
    std::uint16_t telescopeId = 0;     ///< 望远镜编号
    Dss::Core::Timestamp timestamp{};  ///< 采集时间戳
    std::uint16_t ztCode = 0;          ///< 状态/类型编码
    double deltaX = 0.0;               ///< X 方向偏差
    double deltaY = 0.0;               ///< Y 方向偏差
    std::uint8_t pixelScaleX = 0;      ///< X 方向像素尺度
    std::uint8_t pixelScaleY = 0;      ///< Y 方向像素尺度
    double azimuthDegrees = 0.0;       ///< 方位角（度）
    double elevationDegrees = 0.0;     ///< 俯仰角（度）
    double distance = 0.0;             ///< 距离
    double focalLength = 0.0;          ///< 焦距
    double frameRate = 0.0;            ///< 帧率
    double temperature = 0.0;          ///< 温度
    double humidity = 0.0;             ///< 湿度
    double atmosPressure = 0.0;        ///< 大气压
    double windSpeed = 0.0;            ///< 风速
    double cloudCover = 0.0;           ///< 云量
    double exposure = 0.0;             ///< 曝光量
    std::uint32_t width = 0;           ///< 图像宽度（像素）
    std::uint32_t height = 0;          ///< 图像高度（像素）
    std::uint16_t pixelColor = 1;      ///< 像素通道数
    std::uint16_t pixelBit = 16;       ///< 每像素位深
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

/// 解码后的遗留 BMP 文件，包含头部信息与 16 位灰度像素
struct LegacyBmpFile {
    LegacyBmpMetadata metadata{};       ///< 遗留自定义属性与图像尺寸元数据
    BitmapFileHeader fileHeader{};      ///< BMP 文件头
    BitmapInfoHeader infoHeader{};      ///< BMP 信息头
    std::vector<std::uint16_t> pixels;  ///< 按行存储的 16 位灰度像素
};

/**
 * @brief 构建遗留 BMP 自定义属性块（60 字节）
 * @param metadata 源元数据
 * @return 编码后的属性块字节数组
 */
auto buildLegacyBmpAttributeHeader(const LegacyBmpMetadata& metadata)
    -> std::array<std::uint8_t, kLegacyBmpAttributeSize>;

/**
 * @brief 解码遗留 BMP 自定义属性块
 * @param header 属性块字节数据
 * @return 解码后的元数据；格式无效时返回空
 */
auto decodeLegacyBmpAttributeHeader(std::span<const std::uint8_t> header)
    -> std::optional<LegacyBmpMetadata>;

/**
 * @brief 计算遗留 BMP 像素数据的字节数
 * @param metadata 包含宽高与位深信息的元数据
 * @return 像素数据字节数；校验失败时返回空
 */
auto legacyBmpImageByteSize(const LegacyBmpMetadata& metadata) -> std::optional<std::uint32_t>;

/**
 * @brief 根据元数据构建遗留 BMP 文件头
 * @param metadata 源元数据
 * @return 填充偏移与文件大小的文件头结构
 */
auto buildLegacyBitmapFileHeader(const LegacyBmpMetadata& metadata) -> BitmapFileHeader;

/**
 * @brief 根据元数据构建遗留 BMP 信息头
 * @param metadata 源元数据
 * @return 填充尺寸与位深的信息头结构
 */
auto buildLegacyBitmapInfoHeader(const LegacyBmpMetadata& metadata) -> BitmapInfoHeader;

/**
 * @brief 构建遗留 BMP 灰度调色板
 * @param pixelBit 每像素位深
 * @return 256 级灰度调色板；位深大于 8 时返回空向量
 */
auto buildLegacyRgbQuadPalette(std::uint16_t pixelBit) -> std::vector<RgbQuad>;

/**
 * @brief 组装完整的遗留 BMP 文件
 * @param metadata 图像元数据
 * @param imageData 原始像素数据
 * @return 完整 BMP 文件字节流；像素大小不匹配时返回空
 */
auto buildLegacyBmpFile(const LegacyBmpMetadata& metadata, std::span<const std::uint8_t> imageData)
    -> std::optional<std::vector<std::uint8_t>>;

/**
 * @brief 解码 oldsrc/ImageCode 兼容的遗留 BMP 文件
 * @param file 完整 BMP 文件字节流
 * @return 解码后的遗留 BMP 文件；非遗留格式、长度不足或像素布局不支持时返回空
 */
auto decodeLegacyBmpFile(std::span<const std::uint8_t> file) -> std::optional<LegacyBmpFile>;

}  // namespace Dss::Storage
