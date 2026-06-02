#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/comm/serial_protocol_codec.h"

namespace {

void writeU32Le(std::array<uint8_t, 30>& frame, size_t offset, uint32_t value) {
    frame[offset] = static_cast<uint8_t>(value & 0xFFU);
    frame[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    frame[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    frame[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] auto angleCode(double degrees) -> uint32_t {
    return static_cast<uint32_t>(std::llround(degrees / 360.0 * static_cast<double>(1ULL << 29U)));
}

[[nodiscard]] auto makeDisplayLikeFrame(std::size_t size) -> std::array<uint8_t, 30> {
    std::array<uint8_t, 30> frame{};
    frame[0] = Dss::Comm::FrameCodec::HEADER;
    frame[size - 1U] = Dss::Comm::FrameCodec::TAIL;
    frame[1] = 0x26;  // 2026
    frame[2] = 0x06;
    frame[3] = 0x02;
    frame[4] = 0x09;
    frame[5] = 0x58;
    frame[6] = 0x07;
    frame[7] = 0x10;
    frame[8] = 0x27;  // 10000 / 10 = 1000 ms
    writeU32Le(frame, 9U, angleCode(90.0));
    writeU32Le(frame, 13U, angleCode(45.0));
    return frame;
}

}  // namespace

TEST(SerialProtocolCodec, ProvidesExpectedFrameLayouts) {
    const auto display = Dss::Comm::layoutFor(Dss::Comm::SerialProtocol::Display);
    EXPECT_EQ(display.recvSize, 20U);
    EXPECT_EQ(display.sendSize, 9U);

    const auto exposure = Dss::Comm::layoutFor(Dss::Comm::SerialProtocol::Exposure);
    EXPECT_EQ(exposure.recvSize, 23U);
    EXPECT_EQ(exposure.sendSize, 8U);

    const auto master = Dss::Comm::layoutFor(Dss::Comm::SerialProtocol::MasterControl);
    EXPECT_EQ(master.recvSize, 30U);
    EXPECT_EQ(master.sendSize, 28U);

    const auto servo = Dss::Comm::layoutFor(Dss::Comm::SerialProtocol::Servo);
    EXPECT_EQ(servo.recvSize, 20U);
    EXPECT_EQ(servo.sendSize, 14U);
}

TEST(SerialProtocolCodec, ValidatesReceiveFrameByProtocolSize) {
    std::vector<uint8_t> frame(20, 0);
    Dss::Comm::FrameCodec::wrap(frame);

    EXPECT_TRUE(Dss::Comm::validateReceiveFrame(Dss::Comm::SerialProtocol::Display, frame));
    EXPECT_FALSE(Dss::Comm::validateReceiveFrame(Dss::Comm::SerialProtocol::Exposure, frame));
}

TEST(SerialProtocolCodec, PreparesSendFrameByProtocolSize) {
    std::vector<uint8_t> frame(9, 0);

    EXPECT_TRUE(Dss::Comm::prepareSendFrame(Dss::Comm::SerialProtocol::Display, frame));
    EXPECT_TRUE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
    EXPECT_FALSE(Dss::Comm::prepareSendFrame(Dss::Comm::SerialProtocol::Servo, frame));
}

TEST(SerialProtocolCodec, DecodesDisplayFrameTimestampAndPointing) {
    const auto frame = makeDisplayLikeFrame(20U);

    const auto decoded = Dss::Comm::decodeDisplayFrame(std::span<const uint8_t>(frame.data(), 20U));

    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->timestamp.year, 2026);
    EXPECT_EQ(decoded->timestamp.month, 6);
    EXPECT_EQ(decoded->timestamp.day, 2);
    EXPECT_EQ(decoded->timestamp.hour, 9);
    EXPECT_EQ(decoded->timestamp.minute, 58);
    EXPECT_EQ(decoded->timestamp.second, 7);
    EXPECT_EQ(decoded->timestamp.millisecond, 1000);
    EXPECT_NEAR(decoded->pointingAe.x, 90.0F, 1.0e-4F);
    EXPECT_NEAR(decoded->pointingAe.y, 45.0F, 1.0e-4F);
}

TEST(SerialProtocolCodec, DecodesExposureFrameLikeDisplayPointing) {
    const auto frame = makeDisplayLikeFrame(23U);

    const auto decoded =
        Dss::Comm::decodeExposureFrame(std::span<const uint8_t>(frame.data(), 23U));

    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->timestamp.year, 2026);
    EXPECT_NEAR(decoded->pointingAe.x, 90.0F, 1.0e-4F);
    EXPECT_NEAR(decoded->pointingAe.y, 45.0F, 1.0e-4F);
}

TEST(SerialProtocolCodec, EncodesExposureCommand) {
    std::array<uint8_t, 8> frame{};
    const Dss::Comm::ExposureCommand command{
        .triggerMode = Dss::Comm::ExposureTriggerMode::External,
        .frameFrequencyCode = 0x05,
        .exposureDelayTicks = 0x123456U,
    };

    ASSERT_TRUE(Dss::Comm::encodeExposureCommand(command, frame));
    EXPECT_EQ(frame[0], Dss::Comm::FrameCodec::HEADER);
    EXPECT_EQ(frame[1], 0x00);
    EXPECT_EQ(frame[2], 0x05);
    EXPECT_EQ(frame[3], 0x56);
    EXPECT_EQ(frame[4], 0x34);
    EXPECT_EQ(frame[5], 0x12);
    EXPECT_EQ(frame[7], Dss::Comm::FrameCodec::TAIL);
}

TEST(SerialProtocolCodec, DecodesMasterControlCommand) {
    std::array<uint8_t, 30> frame{};
    frame[0] = Dss::Comm::FrameCodec::HEADER;
    frame[29] = Dss::Comm::FrameCodec::TAIL;
    frame[3] = 0x10;
    frame[4] = 0x27;  // 10000 / 100 = 100 ms
    frame[7] = 0x05;
    frame[8] = 0x00;
    frame[9] = 0xFF;
    frame[10] = 0xFF;
    frame[11] = 0x56;
    frame[12] = 0x34;
    frame[13] = 0x12;
    frame[14] = 0x21;
    frame[15] = 0x43;
    frame[16] = 0x65;
    frame[17] = 1;
    frame[18] = 2;
    frame[19] = 3;
    frame[20] = 4;
    frame[21] = 5;
    frame[22] = 6;

    const auto decoded = Dss::Comm::decodeMasterControlFrame(frame);

    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_FLOAT_EQ(decoded->exposure, 100.0F);
    EXPECT_EQ(decoded->mode1, 0x05);
    EXPECT_EQ(decoded->mode2, 0x00);
    EXPECT_EQ(decoded->trackMode, 3);
    EXPECT_TRUE(decoded->track);
    EXPECT_TRUE(decoded->save);
    EXPECT_EQ(decoded->targetId, 0x123456U);
    EXPECT_EQ(decoded->taskId, 0x654321U);
    EXPECT_EQ(decoded->start.hour, 1);
    EXPECT_EQ(decoded->start.minute, 2);
    EXPECT_EQ(decoded->start.second, 3);
    EXPECT_EQ(decoded->end.hour, 4);
    EXPECT_EQ(decoded->end.minute, 5);
    EXPECT_EQ(decoded->end.second, 6);
}

TEST(SerialProtocolCodec, EncodesServoCorrection) {
    std::array<uint8_t, 14> frame{};
    const Dss::Comm::ServoCorrection correction{
        .distanceValid = true,
        .speedValid = true,
        .distanceArcsec = Dss::Core::Vec2f{12.3F, -45.6F},
        .speedArcsecPerSec = Dss::Core::Vec2f{123.0F, -456.0F},
        .mode = 0x19,
    };

    ASSERT_TRUE(Dss::Comm::encodeServoCorrection(correction, frame));
    EXPECT_EQ(frame[0], Dss::Comm::FrameCodec::HEADER);
    EXPECT_EQ(frame[1], 0x11);
    EXPECT_EQ(frame[2], 123);
    EXPECT_EQ(frame[3], 0);
    EXPECT_EQ(frame[4], 0xC8);
    EXPECT_EQ(frame[5], 0x81);
    EXPECT_EQ(frame[6], 123);
    EXPECT_EQ(frame[7], 0);
    EXPECT_EQ(frame[8], 0);
    EXPECT_EQ(frame[9], 0xC8);
    EXPECT_EQ(frame[10], 0x01);
    EXPECT_EQ(frame[11], 0x80);
    EXPECT_EQ(frame[12], 0x19);
    EXPECT_EQ(frame[13], Dss::Comm::FrameCodec::TAIL);
}

TEST(SerialProtocolCodec, EncodesMasterControlStatus) {
    std::array<uint8_t, 28> frame{};
    const Dss::Comm::MasterControlStatus status{
        .correction =
            Dss::Comm::ServoCorrection{
                .distanceValid = true,
                .speedValid = false,
                .distanceArcsec = Dss::Core::Vec2f{-1.2F, 3.4F},
            },
        .timestamp = Dss::Core::Timestamp{.year = 2026,
                                          .month = 6,
                                          .day = 2,
                                          .hour = 9,
                                          .minute = 58,
                                          .second = 7,
                                          .millisecond = 123},
        .pointingAe = Dss::Core::Vec2f{90.0F, 45.0F},
    };

    ASSERT_TRUE(Dss::Comm::encodeMasterControlStatus(status, frame));
    EXPECT_EQ(frame[0], Dss::Comm::FrameCodec::HEADER);
    EXPECT_EQ(frame[1], 0x01);
    EXPECT_EQ(frame[2], 12);
    EXPECT_EQ(frame[3], 0x80);
    EXPECT_EQ(frame[4], 34);
    EXPECT_EQ(frame[5], 0);
    EXPECT_EQ(frame[6], 0x26);
    EXPECT_EQ(frame[7], 0x06);
    EXPECT_EQ(frame[8], 0x02);
    EXPECT_EQ(frame[9], 0x09);
    EXPECT_EQ(frame[10], 0x58);
    EXPECT_EQ(frame[11], 0x07);
    EXPECT_EQ(frame[12], 0xCE);
    EXPECT_EQ(frame[13], 0x04);
    EXPECT_EQ(frame[14], 0x00);
    EXPECT_EQ(frame[15], 0x00);
    EXPECT_EQ(frame[16], 0x00);
    EXPECT_EQ(frame[17], 0x08);
    EXPECT_EQ(frame[18], 0x00);
    EXPECT_EQ(frame[19], 0x00);
    EXPECT_EQ(frame[20], 0x00);
    EXPECT_EQ(frame[21], 0x04);
    EXPECT_EQ(frame[27], Dss::Comm::FrameCodec::TAIL);
}
