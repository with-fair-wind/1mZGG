#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "dss/network/atmos_protocol.h"
#include "dss/network/diagnostic_protocol.h"

namespace {

void appendI32Le(std::vector<uint8_t>& packet, int32_t value) {
    const auto raw = std::bit_cast<uint32_t>(value);
    packet.push_back(static_cast<uint8_t>(raw & 0xFFU));
    packet.push_back(static_cast<uint8_t>((raw >> 8U) & 0xFFU));
    packet.push_back(static_cast<uint8_t>((raw >> 16U) & 0xFFU));
    packet.push_back(static_cast<uint8_t>((raw >> 24U) & 0xFFU));
}

void appendDoubleLe(std::vector<uint8_t>& packet, double value) {
    const auto raw = std::bit_cast<uint64_t>(value);
    for (size_t i = 0; i < 8U; ++i) {
        packet.push_back(static_cast<uint8_t>((raw >> (i * 8U)) & 0xFFU));
    }
}

[[nodiscard]] auto makeAtmosPacket() -> std::vector<uint8_t> {
    std::vector<uint8_t> packet;
    packet.reserve(Dss::Network::AtmosPacketSize);
    appendI32Le(packet, 0x12345678);
    appendDoubleLe(packet, 12.5);
    appendDoubleLe(packet, 0.875);
    appendDoubleLe(packet, 1013.25);
    return packet;
}

}  // namespace

TEST(AtmosProtocol, DecodesLegacyWeatherPacketAndScalesPressure) {
    const auto packet = makeAtmosPacket();

    const auto decoded = Dss::Network::decodeAtmosPacket(packet);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->frameHead, 0x12345678);
    EXPECT_DOUBLE_EQ(decoded->temperature, 12.5);
    EXPECT_DOUBLE_EQ(decoded->humidity, 0.875);
    EXPECT_DOUBLE_EQ(decoded->pressure, 101325.0);
}

TEST(AtmosProtocol, RejectsTruncatedWeatherPacket) {
    const auto packet = makeAtmosPacket();

    const auto decoded = Dss::Network::decodeAtmosPacket(
        std::span<const uint8_t>(packet.data(), packet.size() - 1U));

    EXPECT_FALSE(decoded.has_value());
}

TEST(DiagnosticProtocol, BuildsLegacyDpsStatusJson) {
    const Dss::Network::DiagnosticStatus status{
        .displayCommOk = true,
        .exposureCommOk = false,
        .cameraOk = true,
        .storageOk = false,
        .reservedOk = true,
    };

    const auto json = nlohmann::json::parse(Dss::Network::buildDiagnosticStatusJson(status));

    EXPECT_EQ(json.at("DPS001").get<int>(), 0);
    EXPECT_EQ(json.at("DPS002").get<int>(), 1);
    EXPECT_EQ(json.at("DPS003").get<int>(), 0);
    EXPECT_EQ(json.at("DPS004").get<int>(), 1);
}
