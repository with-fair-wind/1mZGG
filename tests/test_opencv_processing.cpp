#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/processing/opencv_processing_strategy.h"

TEST(OpenCvProcessingStrategyTest, ComputesStatsAndExtractsBrightBlob) {
    Dss::Processing::FramePacket packet{};
    packet.width = 8;
    packet.height = 8;
    packet.rawImage.assign(static_cast<std::size_t>(packet.width) * packet.height, 10);

    packet.rawImage[2 * packet.width + 2] = 1000;
    packet.rawImage[2 * packet.width + 3] = 1000;
    packet.rawImage[3 * packet.width + 2] = 1000;
    packet.rawImage[3 * packet.width + 3] = 1000;

    Dss::Processing::OpenCvProcessingStrategy strategy({
        .thresholdSigma = 1.0,
        .minArea = 1,
        .maxArea = 16,
    });

    const auto result = strategy.process(packet);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.displayImage.size(), packet.rawImage.size());
    EXPECT_DOUBLE_EQ(result.stats.minVal, 10.0);
    EXPECT_DOUBLE_EQ(result.stats.maxVal, 1000.0);
    ASSERT_EQ(result.targetBlobs.size(), 1U);
    EXPECT_FLOAT_EQ(result.targetBlobs.front().area, 4.0f);
    EXPECT_FLOAT_EQ(result.targetBlobs.front().centroid.x, 2.5f);
    EXPECT_FLOAT_EQ(result.targetBlobs.front().centroid.y, 2.5f);
}
