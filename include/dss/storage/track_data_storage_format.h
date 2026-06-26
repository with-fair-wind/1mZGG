#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kLegacyTrackAreaScale = 1.7396483216;  ///< 遗留跟踪数据面积缩放系数

/// 单条跟踪数据记录，对应一行遗留格式文本
struct TrackDataRecord {
    std::uint64_t frameSeq = 0;             ///< 帧序号
    Dss::Core::Timestamp timestamp{};       ///< 帧时间戳
    Dss::Core::Vec2f fovCenterAe{};         ///< 视场中心方位-俯仰（度）
    Dss::Core::Vec2f blobPosition{};        ///< 像斑质心坐标（像素）
    Dss::Core::Vec2f opticCenter{};         ///< 光学中心坐标（像素）
    float area = 0.0F;                      ///< 像斑面积（像素²）
    double exposureTimeMilliseconds = 0.0;  ///< 曝光时间（毫秒）
    double rangeMeters = 1350.0;            ///< 距离（米）
    double magnitude = 13.2;                ///< 星等
};

/// 由通用测量结果数据包构造遗留跟踪数据记录。
///
/// @param packet 通用测量结果数据包。
/// @param defaultOpticCenter 数据包未携带光学中心时使用的默认光学中心。
/// @return 可直接写入遗留文本格式的跟踪数据记录。
[[nodiscard]] inline auto makeTrackDataRecord(const Dss::Core::ResultPacket& packet,
                                              Dss::Core::Vec2f defaultOpticCenter = {})
    -> TrackDataRecord {
    TrackDataRecord record{};
    record.frameSeq = packet.frameSeq;
    record.timestamp = packet.timestamp;
    record.fovCenterAe = packet.fovCenterAe;
    if (record.fovCenterAe.x == 0.0F && record.fovCenterAe.y == 0.0F) {
        record.fovCenterAe = packet.fovCenterAeModified;
    }
    record.blobPosition = packet.targetPosFrame;
    record.opticCenter = packet.opticCenter;
    if (record.opticCenter.x == 0.0F && record.opticCenter.y == 0.0F) {
        record.opticCenter = defaultOpticCenter;
    }
    record.area = packet.blob.area;
    record.exposureTimeMilliseconds = static_cast<double>(packet.exposureTime) * 1000.0;
    if (packet.targetMvGdcl != 0.0F) {
        record.magnitude = packet.targetMvGdcl;
    }
    return record;
}

namespace Detail {

/**
 * @brief 将时间戳格式化为遗留跟踪数据行中的时间字符串
 * @param timestamp 源时间戳
 * @return 形如 "YYYY-MM-DD HH:MM:SS.mmm" 的字符串
 */
inline auto legacyTimestamp(const Dss::Core::Timestamp& timestamp) -> std::string {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << timestamp.year << '-' << std::setw(2)
           << timestamp.month << '-' << std::setw(2) << timestamp.day << ' ';
    output << std::setfill(' ') << std::setw(2) << timestamp.hour << ':';
    output << std::setfill('0') << std::setw(2) << timestamp.minute << ':' << std::setw(2)
           << timestamp.second << '.' << std::setw(3) << timestamp.millisecond;
    return output.str();
}

/**
 * @brief 将浮点值格式化为固定 6 位小数的字符串
 * @param value 原始浮点值
 * @return 固定精度字符串
 */
inline auto fixed6(double value) -> std::string {
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

/**
 * @brief 将浮点值格式化为遗留格式的数值字符串
 * @param value 原始浮点值
 * @return 默认浮点格式字符串
 */
inline auto legacyNumber(double value) -> std::string {
    std::ostringstream output;
    output << std::setprecision(6) << std::defaultfloat << value;
    return output.str();
}

}  // namespace Detail

/**
 * @brief 将跟踪数据记录格式化为遗留文本行
 * @param record 源跟踪数据记录
 * @return 空格分隔的遗留格式文本行
 */
inline auto formatLegacyTrackDataLine(const TrackDataRecord& record) -> std::string {
    const auto deltaX = static_cast<double>(record.blobPosition.x - record.opticCenter.x);
    const auto deltaY = static_cast<double>(record.opticCenter.y - record.blobPosition.y);
    const auto area = static_cast<double>(record.area);
    const auto areaScaled = area * area / 10.0 * kLegacyTrackAreaScale;

    std::ostringstream output;
    output << record.frameSeq << ' ' << Detail::legacyTimestamp(record.timestamp) << ' '
           << Detail::fixed6(record.fovCenterAe.x) << ' ' << Detail::fixed6(record.fovCenterAe.y)
           << ' ' << Detail::legacyNumber(deltaX) << ' ' << Detail::legacyNumber(deltaY) << ' '
           << Detail::legacyNumber(area) << ' ' << Detail::legacyNumber(areaScaled) << ' '
           << Detail::legacyNumber(record.exposureTimeMilliseconds) << ' '
           << Detail::legacyNumber(record.rangeMeters) << ' '
           << Detail::legacyNumber(record.magnitude);
    return output.str();
}

}  // namespace Dss::Storage
