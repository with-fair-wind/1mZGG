#include "dss/comm/serial_protocol_codec.h"

#include <gtest/gtest.h>

#include <vector>

TEST(SerialProtocolCodec, ProvidesExpectedFrameLayouts)
{
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

TEST(SerialProtocolCodec, ValidatesReceiveFrameByProtocolSize)
{
    std::vector<uint8_t> frame(20, 0);
    Dss::Comm::FrameCodec::wrap(frame);

    EXPECT_TRUE(Dss::Comm::validateReceiveFrame(Dss::Comm::SerialProtocol::Display, frame));
    EXPECT_FALSE(Dss::Comm::validateReceiveFrame(Dss::Comm::SerialProtocol::Exposure, frame));
}

TEST(SerialProtocolCodec, PreparesSendFrameByProtocolSize)
{
    std::vector<uint8_t> frame(9, 0);

    EXPECT_TRUE(Dss::Comm::prepareSendFrame(Dss::Comm::SerialProtocol::Display, frame));
    EXPECT_TRUE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
    EXPECT_FALSE(Dss::Comm::prepareSendFrame(Dss::Comm::SerialProtocol::Servo, frame));
}
