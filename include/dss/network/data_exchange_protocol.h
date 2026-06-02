#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Dss::Network {

inline constexpr size_t GxtcHeaderSize = 35;
inline constexpr size_t GxtcTargetSize = 29;
inline constexpr size_t GdclPacketSize = 34;

struct GxtcMetadata {
  int32_t jms1970Centiseconds = 0;
  uint8_t measureStatus = 0;
  uint8_t taskMode = 0;
  uint8_t targetStatus = 0;
  double temperatureCelsius = 0.0;
  double atmospherePressure = 0.0;
  double humidityPercent = 0.0;
};

struct GxtcTarget {
  uint8_t mainFlag = 0xFF;
  int32_t targetId = 0;
  int32_t azimuthArcsec = 0;
  int32_t elevationArcsec = 0;
  int32_t azimuthSpeedArcsecPerSecond = 0;
  int32_t elevationSpeedArcsecPerSecond = 0;
  double magnitude = 0.0;
};

struct GdclMeasurement {
  int32_t jms1970Centiseconds = 0;
  uint8_t measureStatus = 0;
  int32_t targetId = 0;
  uint8_t dataFlag = 1;
  int32_t dn = 0;
  int32_t magnitudeCentivalue = 0;
  int32_t distance = 0;
  int32_t alphaArcsec = 0;
  int32_t deltaArcsec = 0;
  int32_t magnitudeResult = 1;
};

namespace Detail {

inline void appendU8(std::vector<uint8_t>& output, uint8_t value) {
  output.push_back(value);
}

inline void appendI32Le(std::vector<uint8_t>& output, int32_t value) {
  const auto raw = std::bit_cast<uint32_t>(value);
  output.push_back(static_cast<uint8_t>(raw & 0xFFU));
  output.push_back(static_cast<uint8_t>((raw >> 8U) & 0xFFU));
  output.push_back(static_cast<uint8_t>((raw >> 16U) & 0xFFU));
  output.push_back(static_cast<uint8_t>((raw >> 24U) & 0xFFU));
}

inline void appendDoubleLe(std::vector<uint8_t>& output, double value) {
  const auto raw = std::bit_cast<uint64_t>(value);
  for (size_t i = 0; i < 8U; ++i) {
    output.push_back(static_cast<uint8_t>((raw >> (i * 8U)) & 0xFFU));
  }
}

}  // namespace Detail

[[nodiscard]] inline auto buildGxtcPacket(const GxtcMetadata& metadata,
                                          std::span<const GxtcTarget> targets)
    -> std::vector<uint8_t> {
  std::vector<uint8_t> packet;
  packet.reserve(GxtcHeaderSize + targets.size() * GxtcTargetSize);

  Detail::appendI32Le(packet, static_cast<int32_t>(targets.size()));
  Detail::appendI32Le(packet, metadata.jms1970Centiseconds);
  Detail::appendU8(packet, metadata.measureStatus);
  Detail::appendU8(packet, metadata.taskMode);
  Detail::appendU8(packet, metadata.targetStatus);
  Detail::appendDoubleLe(packet, metadata.temperatureCelsius);
  Detail::appendDoubleLe(packet, metadata.atmospherePressure);
  Detail::appendDoubleLe(packet, metadata.humidityPercent);

  for (const auto& target : targets) {
    Detail::appendU8(packet, target.mainFlag);
    Detail::appendI32Le(packet, target.targetId);
    Detail::appendI32Le(packet, target.azimuthArcsec);
    Detail::appendI32Le(packet, target.elevationArcsec);
    Detail::appendI32Le(packet, target.azimuthSpeedArcsecPerSecond);
    Detail::appendI32Le(packet, target.elevationSpeedArcsecPerSecond);
    Detail::appendDoubleLe(packet, target.magnitude);
  }

  return packet;
}

[[nodiscard]] inline auto buildGdclPacket(const GdclMeasurement& measurement)
    -> std::vector<uint8_t> {
  std::vector<uint8_t> packet;
  packet.reserve(GdclPacketSize);

  Detail::appendI32Le(packet, measurement.jms1970Centiseconds);
  Detail::appendU8(packet, measurement.measureStatus);
  Detail::appendI32Le(packet, measurement.targetId);
  Detail::appendU8(packet, measurement.dataFlag);
  Detail::appendI32Le(packet, measurement.dn);
  Detail::appendI32Le(packet, measurement.magnitudeCentivalue);
  Detail::appendI32Le(packet, measurement.distance);
  Detail::appendI32Le(packet, measurement.alphaArcsec);
  Detail::appendI32Le(packet, measurement.deltaArcsec);
  Detail::appendI32Le(packet, measurement.magnitudeResult);

  return packet;
}

}  // namespace Dss::Network
