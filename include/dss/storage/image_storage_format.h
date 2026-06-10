#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kRawImageHeaderSize = 31U;  ///< RAW 图像文件头字节长度

/// RAW 图像元数据，对应 31 字节文件头字段
struct RawImageMetadata {
    std::uint32_t width = 0;                       ///< 图像宽度（像素）
    std::uint32_t height = 0;                      ///< 图像高度（像素）
    Dss::Core::ExposureDisplayData exposure{};     ///< 曝光与指向显示数据
    int triggerMode = 0;                           ///< 触发模式
    double frameFrequency = 0.0;                   ///< 帧频（Hz）
    double exposureTimeMilliseconds = 0.0;         ///< 曝光时间（毫秒）
};

/// 图像存储路径与命名规则参数
struct ImageStorageNaming {
    std::filesystem::path rootPath;     ///< 存储根目录
    std::string startTime;              ///< 会话开始时间字符串
    std::string endTime;                ///< 会话结束时间字符串
    std::string taskId;                 ///< 任务编号
    std::string targetId;               ///< 目标编号
    std::string observatoryId;          ///< 观测站编号
    std::string imageFormat = "raw";    ///< 图像文件扩展名
    bool searchMode = false;            ///< 是否为搜索模式（目标段使用占位符）
};

/// 解码后的 RAW 图像文件内容
struct RawImageFile {
    RawImageMetadata metadata{};        ///< 图像元数据
    std::vector<std::uint16_t> pixels;  ///< 16 位像素数据
};

namespace Detail {

/**
 * @brief 以大端序写入 16 位值
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
inline void writeBigEndian16(std::span<std::uint8_t> output, std::size_t offset,
                             std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>(value & 0xFFU);
}

/**
 * @brief 以大端序写入 24 位值
 * @param output 目标字节缓冲区
 * @param offset 写入起始偏移
 * @param value 待写入值
 */
inline void writeBigEndian24(std::span<std::uint8_t> output, std::size_t offset,
                             std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>(value & 0xFFU);
}

/**
 * @brief 从大端序字节流读取 16 位值
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @return 解码后的 16 位值
 */
inline auto readBigEndian16(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) * 256U +
           static_cast<std::uint32_t>(input[offset + 1]);
}

/**
 * @brief 从大端序字节流读取 24 位值
 * @param input 源字节缓冲区
 * @param offset 读取起始偏移
 * @return 解码后的 24 位值
 */
inline auto readBigEndian24(std::span<const std::uint8_t> input, std::size_t offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(input[offset]) * 65536U +
           static_cast<std::uint32_t>(input[offset + 1]) * 256U +
           static_cast<std::uint32_t>(input[offset + 2]);
}

/**
 * @brief 将浮点值截断为无符号 32 位整数
 * @param value 原始浮点值
 * @return 截断后的整数值
 */
inline auto legacyInteger(double value) -> std::uint32_t {
    return static_cast<std::uint32_t>(value);
}

/**
 * @brief 根据命名规则生成目标段字符串
 * @param naming 命名参数
 * @return 搜索模式下返回占位符 "NNNNNN"，否则返回目标编号
 */
inline auto targetSegment(const ImageStorageNaming& naming) -> std::string {
    if (naming.searchMode) {
        return "NNNNNN";
    }
    return naming.targetId;
}

/**
 * @brief 从开始时间字符串截取指定长度的日期段
 * @param startTime 开始时间字符串
 * @param length 截取长度
 * @return 日期段子串；长度不足时返回空字符串
 */
inline auto dateSegment(const std::string& startTime, std::size_t length) -> std::string {
    if (startTime.size() < length) {
        return {};
    }
    return startTime.substr(0, length);
}

/**
 * @brief 将时间戳格式化为帧文件名时间戳段（YYYYMMDDHHmmssSSS）
 * @param timestamp 源时间戳
 * @return 格式化后的时间字符串
 */
inline auto frameTimestamp(const Dss::Core::Timestamp& timestamp) -> std::string {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << timestamp.year << std::setw(2) << timestamp.month
           << std::setw(2) << timestamp.day << std::setw(2) << timestamp.hour << std::setw(2)
           << timestamp.minute << std::setw(2) << timestamp.second << std::setw(3)
           << timestamp.millisecond;
    return output.str();
}

/**
 * @brief 将帧序号格式化为 6 位零填充字符串
 * @param sequence 帧序号
 * @return 6 位序号字符串
 */
inline auto sequenceString(std::uint64_t sequence) -> std::string {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(6) << sequence;
    return output.str();
}

}  // namespace Detail

/**
 * @brief 构建 RAW 图像 31 字节文件头
 * @param metadata 源元数据
 * @return 编码后的文件头字节数组
 */
inline auto buildRawImageHeader(const RawImageMetadata& metadata)
    -> std::array<std::uint8_t, kRawImageHeaderSize> {
    std::array<std::uint8_t, kRawImageHeaderSize> header{};
    Detail::writeBigEndian16(header, 0, metadata.width);
    Detail::writeBigEndian16(header, 2, metadata.height);
    Detail::writeBigEndian16(header, 4,
                             static_cast<std::uint32_t>(metadata.exposure.timestamp.year));
    header[6] = static_cast<std::uint8_t>(metadata.exposure.timestamp.month);
    header[7] = static_cast<std::uint8_t>(metadata.exposure.timestamp.day);
    header[8] = static_cast<std::uint8_t>(metadata.exposure.timestamp.hour);
    header[9] = static_cast<std::uint8_t>(metadata.exposure.timestamp.minute);
    header[10] = static_cast<std::uint8_t>(metadata.exposure.timestamp.second);
    Detail::writeBigEndian16(header, 11,
                             static_cast<std::uint32_t>(metadata.exposure.timestamp.millisecond));
    Detail::writeBigEndian16(header, 13,
                             static_cast<std::uint32_t>(metadata.exposure.timestamp.microsecond));
    Detail::writeBigEndian24(header, 15,
                             Detail::legacyInteger(metadata.exposure.pointingAe.x * 10000.0));
    Detail::writeBigEndian24(header, 18,
                             Detail::legacyInteger(metadata.exposure.pointingAe.y * 10000.0));
    Detail::writeBigEndian24(header, 21,
                             Detail::legacyInteger(metadata.exposureTimeMilliseconds * 1000.0));
    header[24] = static_cast<std::uint8_t>(metadata.triggerMode);
    Detail::writeBigEndian16(header, 25, Detail::legacyInteger(metadata.frameFrequency * 1000.0));
    header[27] =
        static_cast<std::uint8_t>(Detail::legacyInteger(metadata.exposure.temperature + 100.0));
    Detail::writeBigEndian16(header, 28,
                             Detail::legacyInteger(metadata.exposure.atmosPressure / 10.0));
    header[30] =
        static_cast<std::uint8_t>(Detail::legacyInteger(metadata.exposure.humidity * 100.0));
    return header;
}

/**
 * @brief 组装完整的 RAW 图像文件字节流
 * @param metadata 图像元数据
 * @param pixels 16 位像素数据
 * @return 包含文件头与像素数据的完整字节流
 */
inline auto buildRawImageFile(const RawImageMetadata& metadata,
                              std::span<const std::uint16_t> pixels) -> std::vector<std::uint8_t> {
    const auto header = buildRawImageHeader(metadata);
    std::vector<std::uint8_t> file;
    file.reserve(header.size() + pixels.size() * sizeof(std::uint16_t));
    file.insert(file.end(), header.begin(), header.end());
    for (const auto pixel : pixels) {
        file.push_back(static_cast<std::uint8_t>(pixel & 0xFFU));
        file.push_back(static_cast<std::uint8_t>((pixel >> 8) & 0xFFU));
    }
    return file;
}

/**
 * @brief 解码 RAW 图像文件头
 * @param header 文件头字节数据（至少 31 字节）
 * @return 解码后的元数据；长度不足时返回空
 */
inline auto decodeRawImageHeader(std::span<const std::uint8_t> header)
    -> std::optional<RawImageMetadata> {
    if (header.size() < kRawImageHeaderSize) {
        return std::nullopt;
    }

    RawImageMetadata metadata{};
    metadata.width = Detail::readBigEndian16(header, 0);
    metadata.height = Detail::readBigEndian16(header, 2);
    metadata.exposure.timestamp.year = static_cast<int>(Detail::readBigEndian16(header, 4));
    metadata.exposure.timestamp.month = static_cast<int>(header[6]);
    metadata.exposure.timestamp.day = static_cast<int>(header[7]);
    metadata.exposure.timestamp.hour = static_cast<int>(header[8]);
    metadata.exposure.timestamp.minute = static_cast<int>(header[9]);
    metadata.exposure.timestamp.second = static_cast<int>(header[10]);
    metadata.exposure.timestamp.millisecond = static_cast<int>(Detail::readBigEndian16(header, 11));
    metadata.exposure.timestamp.microsecond = static_cast<int>(Detail::readBigEndian16(header, 13));
    metadata.exposure.pointingAe.x =
        static_cast<float>(Detail::readBigEndian24(header, 15) / 10000.0);
    metadata.exposure.pointingAe.y =
        static_cast<float>(Detail::readBigEndian24(header, 18) / 10000.0);
    metadata.exposureTimeMilliseconds = Detail::readBigEndian16(header, 21) / 1000.0;
    metadata.triggerMode = static_cast<int>(header[24]);
    metadata.frameFrequency = Detail::readBigEndian16(header, 25) / 1000.0;
    if (header[27] != 0U) {
        metadata.exposure.temperature = static_cast<double>(header[27]) - 100.0;
    }
    if (header[28] != 0U && header[29] != 0U) {
        metadata.exposure.atmosPressure = Detail::readBigEndian16(header, 28) * 10.0;
    }
    if (header[30] != 0U) {
        metadata.exposure.humidity = static_cast<double>(header[30]) / 100.0;
    }
    return metadata;
}

/**
 * @brief 解码完整的 RAW 图像文件
 * @param file 完整文件字节数据
 * @return 解码后的图像文件结构；格式或大小无效时返回空
 */
inline auto decodeRawImageFile(std::span<const std::uint8_t> file) -> std::optional<RawImageFile> {
    const auto metadata = decodeRawImageHeader(file);
    if (!metadata.has_value()) {
        return std::nullopt;
    }
    const auto pixelCount =
        static_cast<std::uint64_t>(metadata->width) * static_cast<std::uint64_t>(metadata->height);
    const auto payloadBytes = pixelCount * sizeof(std::uint16_t);
    if (pixelCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        payloadBytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        file.size() < kRawImageHeaderSize + static_cast<std::size_t>(payloadBytes)) {
        return std::nullopt;
    }

    RawImageFile decoded{};
    decoded.metadata = *metadata;
    decoded.pixels.reserve(static_cast<std::size_t>(pixelCount));
    for (std::size_t offset = kRawImageHeaderSize;
         offset < kRawImageHeaderSize + static_cast<std::size_t>(payloadBytes); offset += 2) {
        decoded.pixels.push_back(
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(file[offset]) |
                                       static_cast<std::uint16_t>(file[offset + 1] << 8)));
    }
    return decoded;
}

/**
 * @brief 根据帧频计算回放间隔（毫秒）
 * @param frameFrequency 帧频（Hz）
 * @return 回放间隔毫秒数；帧频无效时返回空
 */
inline auto replayIntervalMilliseconds(double frameFrequency) -> std::optional<int> {
    if (frameFrequency <= 0.0) {
        return std::nullopt;
    }
    return static_cast<int>(1000.0 / frameFrequency);
}

/**
 * @brief 从文件头帧频字段计算回放间隔（毫秒）
 * @param frameFrequency 文件头中 2 字节大端帧频字段
 * @return 回放间隔毫秒数；帧频无效时返回空
 */
inline auto replayIntervalMilliseconds(std::span<const std::uint8_t, 2> frameFrequency)
    -> std::optional<int> {
    return replayIntervalMilliseconds(Detail::readBigEndian16(frameFrequency, 0) / 1000.0);
}

/**
 * @brief 构建会话级存储目录路径
 * @param naming 命名参数
 * @return 形如 root/YYYY/YYYYMM/YYYYMMDD/startTime_target_observatory.BMP 的路径
 */
inline auto buildSessionPath(const ImageStorageNaming& naming) -> std::filesystem::path {
    auto path = naming.rootPath;
    const auto yyyy = Detail::dateSegment(naming.startTime, 4);
    const auto yyyymm = Detail::dateSegment(naming.startTime, 6);
    const auto yyyymmdd = Detail::dateSegment(naming.startTime, 8);
    if (!yyyy.empty() && !yyyymm.empty() && !yyyymmdd.empty()) {
        path /= yyyy;
        path /= yyyymm;
        path /= yyyymmdd;
    }
    path /= naming.startTime + "_" + Detail::targetSegment(naming) + "_" + naming.observatoryId +
            ".BMP";
    return path;
}

/**
 * @brief 构建单帧图像文件的完整路径
 * @param naming 命名参数
 * @param metadata 帧元数据（用于提取时间戳）
 * @param sequence 帧序号
 * @return 会话目录下的单帧文件路径
 */
inline auto buildImageFilePath(const ImageStorageNaming& naming, const RawImageMetadata& metadata,
                               std::uint64_t sequence) -> std::filesystem::path {
    auto path = buildSessionPath(naming);
    path /= Detail::frameTimestamp(metadata.exposure.timestamp) + "_" + naming.targetId + "_" +
            naming.observatoryId + "_" + Detail::sequenceString(sequence) + "." +
            naming.imageFormat;
    return path;
}

/**
 * @brief 构建索引文件名（.IMI）
 * @param naming 命名参数
 * @return 索引文件名
 */
inline auto buildIfmFileName(const ImageStorageNaming& naming) -> std::string {
    return naming.startTime + "_" + Detail::targetSegment(naming) + "_" + naming.observatoryId +
           ".IMI";
}

/**
 * @brief 构建索引文件（.IMI）内容
 * @param naming 命名参数
 * @return 索引文件文本内容
 */
inline auto buildIfmContent(const ImageStorageNaming& naming) -> std::string {
    const auto fileName = buildIfmFileName(naming);
    std::ostringstream output;
    output << "C " << fileName << '\n';
    output << "C " << naming.taskId << '\n';
    output << "C " << naming.startTime << '\n';
    output << "C" << naming.endTime << '\n';
    output << "C " << '\n';
    output << "C " << naming.observatoryId << '\n';
    output << "C " << '\n';
    output << "C " << '\n';
    output << "C " << '\n';
    output << "C " << '\n';
    output << "E" << '\n';
    return output.str();
}

}  // namespace Dss::Storage
