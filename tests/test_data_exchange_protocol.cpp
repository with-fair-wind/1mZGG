#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/types.h"
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

TEST(DataExchangeProtocol, MapsResultPacketToGxtcDtosWithLegacyScaling) {
    Dss::Core::ResultPacket result{};
    result.targetId = "geo-42";
    result.valid = true;
    result.targetPosZxdw = Dss::Core::Vec2f{1.25F, -2.5F};
    result.targetSpdAe = Dss::Core::Vec2f{0.125F, -0.25F};
    result.targetMvGdcl = 8.75F;
    result.temperature = 12.5F;
    result.humidity = 0.65F;
    result.atmosPressure = 101325.0F;

    const Dss::Network::DataExchangeMappingOptions options{
        .jms1970Centiseconds = 123456,
        .measureStatus = 1,
    };

    const auto metadata = Dss::Network::makeGxtcMetadata(result, options);
    const auto target = Dss::Network::makeGxtcTarget(result, options);

    EXPECT_EQ(metadata.jms1970Centiseconds, 123456);
    EXPECT_EQ(metadata.measureStatus, 1U);
    EXPECT_EQ(metadata.taskMode, 2U);
    EXPECT_EQ(metadata.targetStatus, 1U);
    EXPECT_DOUBLE_EQ(metadata.temperatureCelsius, 12.5);
    EXPECT_DOUBLE_EQ(metadata.atmospherePressure, 101325.0);
    EXPECT_NEAR(metadata.humidityPercent, 65.0, 1.0e-5);

    EXPECT_EQ(target.mainFlag, 0xFFU);
    EXPECT_EQ(target.targetId, 42);
    EXPECT_EQ(target.azimuthArcsec, 4500);
    EXPECT_EQ(target.elevationArcsec, -9000);
    EXPECT_EQ(target.azimuthSpeedArcsecPerSecond, 450);
    EXPECT_EQ(target.elevationSpeedArcsecPerSecond, -900);
    EXPECT_DOUBLE_EQ(target.magnitude, 0.0);
}

TEST(DataExchangeProtocol, MapsResultPacketToGdclMeasurementWithLegacyScaling) {
    Dss::Core::ResultPacket result{};
    result.targetId = "leo-17";
    result.blob.dn = 4096.75F;
    result.targetMvGdcl = -1.25F;
    result.targetPosTwdw = Dss::Core::Vec2f{-0.1F, 0.2F};

    const Dss::Network::DataExchangeMappingOptions options{
        .jms1970Centiseconds = 654321,
        .measureStatus = 2,
        .distance = 123,
    };

    const auto measurement = Dss::Network::makeGdclMeasurement(result, options);

    EXPECT_EQ(measurement.jms1970Centiseconds, 654321);
    EXPECT_EQ(measurement.measureStatus, 2U);
    EXPECT_EQ(measurement.targetId, 17);
    EXPECT_EQ(measurement.dataFlag, 1U);
    EXPECT_EQ(measurement.dn, 4096);
    EXPECT_EQ(measurement.magnitudeCentivalue, -125);
    EXPECT_EQ(measurement.distance, 123);
    EXPECT_EQ(measurement.alphaArcsec, -360);
    EXPECT_EQ(measurement.deltaArcsec, 720);
    EXPECT_EQ(measurement.magnitudeResult, 1);
}

TEST(DataExchangeProtocol, UsesFallbackTargetIdWhenNameHasNoNumericSuffix) {
    Dss::Core::ResultPacket result{};
    result.targetId = "manual";

    const Dss::Network::DataExchangeMappingOptions options{
        .fallbackTargetId = 77,
    };

    EXPECT_EQ(Dss::Network::parseLegacyTargetId(result.targetId, options.fallbackTargetId), 77);
    EXPECT_EQ(Dss::Network::makeGxtcTarget(result, options).targetId, 77);
    EXPECT_EQ(Dss::Network::makeGdclMeasurement(result, options).targetId, 77);
}

TEST(DataExchangeProtocol, UsesFallbackTargetIdWhenNameDoesNotEndWithNumber) {
    EXPECT_EQ(Dss::Network::parseLegacyTargetId("geo-42x", 77), 77);
}

TEST(DataExchangeProtocol, ConvertsTimestampToJms1970Centiseconds) {
    const Dss::Core::Timestamp timestamp{
        .year = 1970,
        .month = 1,
        .day = 2,
        .hour = 0,
        .minute = 0,
        .second = 1,
        .millisecond = 230,
    };

    EXPECT_EQ(Dss::Network::makeJms1970Centiseconds(timestamp), 8640123);
}

TEST(DataExchangeProtocol, ReturnsZeroJmsForInvalidTimestamp) {
    EXPECT_EQ(Dss::Network::makeJms1970Centiseconds(Dss::Core::Timestamp{}), 0);
}
