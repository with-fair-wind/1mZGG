#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include "dss/core/types.h"

namespace Dss::Storage {

inline constexpr auto kLegacyTrackAreaScale = 1.7396483216;

struct TrackDataRecord {
    std::uint64_t frameSeq = 0;
    Dss::Core::Timestamp timestamp{};
    Dss::Core::Vec2f fovCenterAe{};
    Dss::Core::Vec2f blobPosition{};
    Dss::Core::Vec2f opticCenter{};
    float area = 0.0F;
    double exposureTimeMilliseconds = 0.0;
    double rangeMeters = 1350.0;
    double magnitude = 13.2;
};

namespace Detail {

inline auto legacyTimestamp(const Dss::Core::Timestamp& timestamp) -> std::string {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << timestamp.year << '-' << std::setw(2)
           << timestamp.month << '-' << std::setw(2) << timestamp.day << ' ';
    output << std::setfill(' ') << std::setw(2) << timestamp.hour << ':';
    output << std::setfill('0') << std::setw(2) << timestamp.minute << ':' << std::setw(2)
           << timestamp.second << '.' << std::setw(3) << timestamp.millisecond;
    return output.str();
}

inline auto fixed6(double value) -> std::string {
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

inline auto legacyNumber(double value) -> std::string {
    std::ostringstream output;
    output << std::setprecision(6) << std::defaultfloat << value;
    return output.str();
}

}  // namespace Detail

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
