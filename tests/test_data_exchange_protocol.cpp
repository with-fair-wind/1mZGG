#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/network/data_exchange_protocol.h"

namespace {

[[nodiscard]] auto readI32Le(const std::vector<uint8_t>& packet, size_t offset) -> int32_t {
    const auto raw = static_cast<uint32_t>(packet[offset]) |
                     (static_cast<uint32_t>(packet[offset + 1U]) << 8U) |
                     (static_cast<uint32_t>(packet[offset + 2U]) << 16U) |
                     (static_cast<uint32_t>(packet[offset + 3U]) << 24U);
    return std::bit_cast<int32_t>(raw);
}

[[nodiscard]] auto readDoubleLe(const std::vector<uint8_t>& packet, size_t offset) -> double {
    uint64_t raw = 0;
    for (size_t i = 0; i < 8U; ++i) {
        raw |= static_cast<uint64_t>(packet[offset + i]) << (i * 8U);
    }
    return std::bit_cast<double>(raw);
}

}  // namespace

TEST(DataExchangeProtocol, UsesLegacyFixedSizes) {
    EXPECT_EQ(Dss::Network::GxtcHeaderSize, 35U);
    EXPECT_EQ(Dss::Network::GxtcTargetSize, 29U);
    EXPECT_EQ(Dss::Network::GdclPacketSize, 34U);
}

TEST(DataExchangeProtocol, BuildsGxtcPacketWithLegacyPackedLayout) {
    const Dss::Network::GxtcMetadata metadata{
        .jms1970Centiseconds = 123456,
        .measureStatus = 1,
        .taskMode = 2,
        .targetStatus = 1,
        .temperatureCelsius = 12.5,
        .atmospherePressure = 101325.25,
        .humidityPercent = 65.5,
    };
    const std::vector<Dss::Network::GxtcTarget> targets{
        {
            .mainFlag = 0xFF,
            .targetId = 42,
            .azimuthArcsec = 3600,
            .elevationArcsec = -7200,
            .azimuthSpeedArcsecPerSecond = 15,
            .elevationSpeedArcsecPerSecond = -16,
            .magnitude = 1.5,
        },
        {
            .mainFlag = 0x7F,
            .targetId = 77,
            .azimuthArcsec = -1,
            .elevationArcsec = 2,
            .azimuthSpeedArcsecPerSecond = -3,
            .elevationSpeedArcsecPerSecond = 4,
            .magnitude = -0.25,
        },
    };

    const auto packet = Dss::Network::buildGxtcPacket(metadata, targets);

    ASSERT_EQ(packet.size(),
              Dss::Network::GxtcHeaderSize + targets.size() * Dss::Network::GxtcTargetSize);
    EXPECT_EQ(readI32Le(packet, 0), 2);
    EXPECT_EQ(readI32Le(packet, 4), 123456);
    EXPECT_EQ(packet[8], 1U);
    EXPECT_EQ(packet[9], 2U);
    EXPECT_EQ(packet[10], 1U);
    EXPECT_DOUBLE_EQ(readDoubleLe(packet, 11), 12.5);
    EXPECT_DOUBLE_EQ(readDoubleLe(packet, 19), 101325.25);
    EXPECT_DOUBLE_EQ(readDoubleLe(packet, 27), 65.5);

    const size_t targetOffset = Dss::Network::GxtcHeaderSize;
    EXPECT_EQ(packet[targetOffset], 0xFFU);
    EXPECT_EQ(readI32Le(packet, targetOffset + 1U), 42);
    EXPECT_EQ(readI32Le(packet, targetOffset + 5U), 3600);
    EXPECT_EQ(readI32Le(packet, targetOffset + 9U), -7200);
    EXPECT_EQ(readI32Le(packet, targetOffset + 13U), 15);
    EXPECT_EQ(readI32Le(packet, targetOffset + 17U), -16);
    EXPECT_DOUBLE_EQ(readDoubleLe(packet, targetOffset + 21U), 1.5);

    const size_t secondOffset = targetOffset + Dss::Network::GxtcTargetSize;
    EXPECT_EQ(packet[secondOffset], 0x7FU);
    EXPECT_EQ(readI32Le(packet, secondOffset + 1U), 77);
    EXPECT_EQ(readI32Le(packet, secondOffset + 5U), -1);
    EXPECT_EQ(readI32Le(packet, secondOffset + 9U), 2);
    EXPECT_EQ(readI32Le(packet, secondOffset + 13U), -3);
    EXPECT_EQ(readI32Le(packet, secondOffset + 17U), 4);
    EXPECT_DOUBLE_EQ(readDoubleLe(packet, secondOffset + 21U), -0.25);
}

TEST(DataExchangeProtocol, BuildsGdclPacketWithLegacyPackedLayout) {
    const Dss::Network::GdclMeasurement measurement{
        .jms1970Centiseconds = 654321,
        .measureStatus = 2,
        .targetId = 42,
        .dataFlag = 1,
        .dn = 4096,
        .magnitudeCentivalue = -125,
        .distance = 123,
        .alphaArcsec = -360,
        .deltaArcsec = 720,
        .magnitudeResult = 1,
    };

    const auto packet = Dss::Network::buildGdclPacket(measurement);

    ASSERT_EQ(packet.size(), Dss::Network::GdclPacketSize);
    EXPECT_EQ(readI32Le(packet, 0), 654321);
    EXPECT_EQ(packet[4], 2U);
    EXPECT_EQ(readI32Le(packet, 5), 42);
    EXPECT_EQ(packet[9], 1U);
    EXPECT_EQ(readI32Le(packet, 10), 4096);
    EXPECT_EQ(readI32Le(packet, 14), -125);
    EXPECT_EQ(readI32Le(packet, 18), 123);
    EXPECT_EQ(readI32Le(packet, 22), -360);
    EXPECT_EQ(readI32Le(packet, 26), 720);
    EXPECT_EQ(readI32Le(packet, 30), 1);
}
