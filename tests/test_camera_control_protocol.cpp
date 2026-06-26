#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/camera_control_protocol.h"

namespace {

using Dss::Acquisition::CameraCommand;

}  // namespace

TEST(CameraControlProtocol, BuildsLegacyGrabCommands) {
    EXPECT_EQ(Dss::Acquisition::buildGrabCommand(true), (CameraCommand{0x51, 0x47, 0x03}));
    EXPECT_EQ(Dss::Acquisition::buildGrabCommand(false), (CameraCommand{0x51, 0x07, 0x03}));
}

TEST(CameraControlProtocol, EncodesRegisterCommandLikeCommCamera) {
    EXPECT_EQ(Dss::Acquisition::encodeRegisterCommand(2, 0, 0x1C),
              (CameraCommand{0x09, 0x03, 0x1F}));
    EXPECT_EQ(Dss::Acquisition::encodeRegisterCommand(1, 0x0F, 0xE2),
              (CameraCommand{0xF5, 0x83, 0xE3}));
}

TEST(CameraControlProtocol, BuildsExposureCommandsWithLegacyMinimum) {
    const auto commands = Dss::Acquisition::buildExposureCommands(1.0F);

    ASSERT_EQ(commands.size(), 3U);
    EXPECT_EQ(commands[0], (CameraCommand{0x09, 0x03, 0x1F}));
    EXPECT_EQ(commands[1], (CameraCommand{0x19, 0x03, 0x03}));
    EXPECT_EQ(commands[2], (CameraCommand{0x29, 0x03, 0x03}));
}

TEST(CameraControlProtocol, BuildsLegacyFixedGainCommands) {
    const auto commands = Dss::Acquisition::buildGainCommands(4.0F);

    ASSERT_EQ(commands.size(), 3U);
    EXPECT_EQ(commands[0], (CameraCommand{0xF5, 0x83, 0xE3}));
    EXPECT_EQ(commands[1], (CameraCommand{0x05, 0x07, 0x33}));
    EXPECT_EQ(commands[2], (CameraCommand{0x55, 0x0B, 0x1F}));
}

TEST(CameraControlProtocol, BuildsWindowCommandsInLegacyOrder) {
    const auto fullWidthCommands = Dss::Acquisition::buildWindowCommands(6144, 128, 6144, 1024);
    const auto subWidthCommands = Dss::Acquisition::buildWindowCommands(1024, 128, 6144, 1024);

    ASSERT_EQ(fullWidthCommands.size(), 4U);
    ASSERT_EQ(subWidthCommands.size(), 4U);
    EXPECT_EQ(fullWidthCommands[0], Dss::Acquisition::encodeRegisterCommand(2, 4, 0x80));
    EXPECT_EQ(fullWidthCommands[1], Dss::Acquisition::encodeRegisterCommand(2, 5, 0x00));
    EXPECT_EQ(fullWidthCommands[2], Dss::Acquisition::encodeRegisterCommand(2, 6, 0x00));
    EXPECT_EQ(fullWidthCommands[3], Dss::Acquisition::encodeRegisterCommand(2, 7, 0x18));
    EXPECT_EQ(subWidthCommands[0], Dss::Acquisition::encodeRegisterCommand(2, 6, 0x00));
    EXPECT_EQ(subWidthCommands[1], Dss::Acquisition::encodeRegisterCommand(2, 7, 0x04));
    EXPECT_EQ(subWidthCommands[2], Dss::Acquisition::encodeRegisterCommand(2, 4, 0x80));
    EXPECT_EQ(subWidthCommands[3], Dss::Acquisition::encodeRegisterCommand(2, 5, 0x00));
}

TEST(CameraControlProtocol, DecodesLegacyTemperatureBytes) {
    const std::array<uint8_t, 3> positive{0x1B, 0x48, 0x00};
    const std::array<uint8_t, 3> negative{0xFF, 0xD8, 0x00};

    const auto positiveTemp = Dss::Acquisition::decodeTemperature(positive);
    const auto negativeTemp = Dss::Acquisition::decodeTemperature(negative);

    ASSERT_TRUE(positiveTemp.has_value());
    ASSERT_TRUE(negativeTemp.has_value());
    EXPECT_FLOAT_EQ(*positiveTemp, 12.3F);
    EXPECT_FLOAT_EQ(*negativeTemp, -75.5F);
}

TEST(CameraControlProtocol, RejectsTruncatedTemperatureBytes) {
    const std::array<uint8_t, 2> truncated{0x1B, 0x48};

    EXPECT_FALSE(Dss::Acquisition::decodeTemperature(truncated).has_value());
}
