#include <string_view>
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

TEST(FrameCodecTest, ReportsDetailedValidationFailure) {
    const std::vector<uint8_t> wrongHeader{0x00, 0x01, Dss::Core::FrameTail};

    const auto headerResult = Dss::Comm::FrameCodec::validateDetailed(wrongHeader, 3U);
    EXPECT_FALSE(headerResult.valid);
    EXPECT_EQ(headerResult.failure, Dss::Comm::FrameValidationFailure::HeaderMismatch);
    EXPECT_EQ(headerResult.expectedSize, 3U);
    EXPECT_EQ(headerResult.actualSize, 3U);
    EXPECT_EQ(headerResult.observedHeader, 0x00);
    EXPECT_EQ(headerResult.observedTail, Dss::Core::FrameTail);
    EXPECT_EQ(Dss::Comm::FrameCodec::failureMessage(headerResult.failure),
              std::string_view("serial frame header mismatch"));

    const auto sizeResult = Dss::Comm::FrameCodec::validateDetailed(wrongHeader, 4U);
    EXPECT_FALSE(sizeResult.valid);
    EXPECT_EQ(sizeResult.failure, Dss::Comm::FrameValidationFailure::SizeMismatch);
    EXPECT_EQ(sizeResult.expectedSize, 4U);
    EXPECT_EQ(sizeResult.actualSize, 3U);
}

TEST(FrameCodecTest, RejectsEmptyFrame) {
    const std::vector<uint8_t> frame;

    EXPECT_FALSE(Dss::Comm::FrameCodec::validate(frame, frame.size()));
}
