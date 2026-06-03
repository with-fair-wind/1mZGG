#include <gtest/gtest.h>

#include "dss/core/constants.h"
#include "dss/network/heartbeat.h"

TEST(Heartbeat, BuildsHeartbeatFrame) {
    const auto frame = Dss::Network::Heartbeat::buildFrame();

    ASSERT_EQ(frame.size(), 10U);
    EXPECT_EQ(frame.front(), Dss::Core::FrameHeader);
    EXPECT_EQ(frame.back(), Dss::Core::FrameTail);
    EXPECT_EQ(frame[1], 0x00);
}

TEST(Heartbeat, BuildsCloseGuardFrame) {
    const auto frame = Dss::Network::Heartbeat::buildCloseGuardFrame();

    ASSERT_EQ(frame.size(), 10U);
    EXPECT_EQ(frame.front(), Dss::Core::FrameHeader);
    EXPECT_EQ(frame.back(), Dss::Core::FrameTail);
    EXPECT_EQ(frame[1], 0x01);
}
