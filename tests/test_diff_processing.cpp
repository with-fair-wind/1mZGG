#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dss/processing/diff_processing_strategy.h"

namespace {

auto makeFrame(std::vector<std::uint16_t> pixels, std::uint32_t width, std::uint32_t height)
    -> Dss::Processing::FramePacket {
    Dss::Processing::FramePacket frame{};
    frame.width = width;
    frame.height = height;
    frame.rawImage = std::move(pixels);
    return frame;
}

}  // namespace

TEST(DiffProcessing, FirstFrameSeedsHistoryAndSecondFrameDetectsAbsoluteDifference) {
    Dss::Processing::DiffProcessingStrategy strategy({
        .threshold = 20,
        .minArea = 1,
        .maxArea = 4,
    });

    const auto first = strategy.process(makeFrame({10, 10, 10, 10}, 2, 2));
    const auto second = strategy.process(makeFrame({10, 40, 10, 40}, 2, 2));

    ASSERT_TRUE(first.success);
    EXPECT_TRUE(first.targetBlobs.empty());
    EXPECT_EQ(first.displayImage, std::vector<std::uint8_t>({0, 0, 0, 0}));
    ASSERT_TRUE(second.success);
    EXPECT_EQ(second.displayImage, std::vector<std::uint8_t>({0, 255, 0, 255}));
    ASSERT_EQ(second.targetBlobs.size(), 1U);
    EXPECT_FLOAT_EQ(second.targetBlobs.front().area, 2.0F);
}

TEST(DiffProcessing, ResetsHistoryWhenFrameDimensionsChange) {
    Dss::Processing::DiffProcessingStrategy strategy({.threshold = 5, .minArea = 1});
    ASSERT_TRUE(strategy.process(makeFrame({0, 0, 0, 0}, 2, 2)).success);

    const auto resized = strategy.process(makeFrame({100, 100, 100}, 3, 1));

    ASSERT_TRUE(resized.success);
    EXPECT_EQ(resized.displayImage, std::vector<std::uint8_t>({0, 0, 0}));
    EXPECT_TRUE(resized.targetBlobs.empty());
}

TEST(DiffProcessing, RejectsInvalidPixelCountWithoutReplacingHistory) {
    Dss::Processing::DiffProcessingStrategy strategy({.threshold = 5, .minArea = 1});
    ASSERT_TRUE(strategy.process(makeFrame({10, 10, 10, 10}, 2, 2)).success);

    EXPECT_FALSE(strategy.process(makeFrame({20, 20, 20}, 2, 2)).success);
    const auto next = strategy.process(makeFrame({10, 20, 10, 20}, 2, 2));

    ASSERT_TRUE(next.success);
    EXPECT_EQ(next.displayImage, std::vector<std::uint8_t>({0, 255, 0, 255}));
}
