#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "dss/processing/cuda_processing_strategy.h"
#if defined(DSS_HAS_CUDA) && defined(DSS_HAS_OPENCV)
#include "dss/processing/opencv_processing_strategy.h"
#endif

TEST(CudaProcessingContract, UnavailableBuildReturnsErrorWithoutThrowing) {
#ifndef DSS_HAS_CUDA
    const auto strategy = Dss::Processing::createCudaProcessingStrategy();

    ASSERT_FALSE(strategy.has_value());
    EXPECT_NE(strategy.error().find("not available"), std::string::npos);
#else
    GTEST_SKIP() << "Covered by CUDA-enabled contract";
#endif
}

#if defined(DSS_HAS_CUDA) && defined(DSS_HAS_OPENCV)
TEST(CudaProcessingContract, MatchesOpenCvBlobGeometryOnFixedFrame) {
    auto cuda = Dss::Processing::createCudaProcessingStrategy({
        .thresholdSigma = 1.0,
        .minArea = 1,
        .maxArea = 16,
    });
    if (!cuda) {
        GTEST_SKIP() << cuda.error();
    }

    Dss::Processing::FramePacket packet{};
    packet.width = 8;
    packet.height = 8;
    packet.rawImage.assign(static_cast<std::size_t>(packet.width) * packet.height, uint16_t{10});
    packet.rawImage[2 * packet.width + 2] = 1000;
    packet.rawImage[2 * packet.width + 3] = 1000;
    packet.rawImage[3 * packet.width + 2] = 1000;
    packet.rawImage[3 * packet.width + 3] = 1000;

    Dss::Processing::OpenCvProcessingStrategy cpu({
        .thresholdSigma = 1.0,
        .minArea = 1,
        .maxArea = 16,
    });
    const auto expected = cpu.process(packet);
    const auto actual = (*cuda)->process(packet);

    ASSERT_TRUE(actual.success);
    ASSERT_EQ(actual.targetBlobs.size(), expected.targetBlobs.size());
    ASSERT_EQ(actual.targetBlobs.size(), 1U);
    EXPECT_NEAR(actual.targetBlobs[0].area, expected.targetBlobs[0].area, 0.01);
    EXPECT_NEAR(actual.targetBlobs[0].centroid.x, expected.targetBlobs[0].centroid.x, 0.01);
    EXPECT_NEAR(actual.targetBlobs[0].centroid.y, expected.targetBlobs[0].centroid.y, 0.01);
}
#endif