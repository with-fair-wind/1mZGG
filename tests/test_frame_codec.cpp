#include <vector>

#include <gtest/gtest.h>

#include "dss/comm/frame_codec.h"

TEST(FrameCodecTest, WrapsAndValidatesFrame) {
    std::vector<uint8_t> frame(4, 0);

    Dss::Comm::FrameCodec::wrap(frame);

    EXPECT_EQ(frame.front(), Dss::Core::FrameHeader);
    EXPECT_EQ(frame.back(), Dss::Core::FrameTail);
    EXPECT_TRUE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
}

TEST(FrameCodecTest, RejectsWrongTailOrSize) {
    std::vector<uint8_t> frame{Dss::Core::FrameHeader, 0x01, 0x02, 0x00};

    EXPECT_FALSE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
    EXPECT_FALSE(Dss::Comm::FrameCodec::validate(frame, frame.size() + 1));
}

TEST(FrameCodecTest, RejectsEmptyFrame) {
    const std::vector<uint8_t> frame;

    EXPECT_FALSE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
}
