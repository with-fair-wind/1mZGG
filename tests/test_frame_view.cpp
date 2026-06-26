#include <gtest/gtest.h>

#include "dss/processing/frame_view.h"

TEST(FrameView, ExposesNonOwningImageSpans) {
    Dss::Processing::FramePacket packet;
    packet.frameSeq = 42;
    packet.width = 2;
    packet.height = 2;
    packet.rawImage = {1, 2, 3, 4};
    packet.displayImage = {10, 20, 30, 40};

    const auto view = Dss::Processing::makeFrameView(packet);

    EXPECT_EQ(view.frameSeq, 42U);
    EXPECT_EQ(view.width, 2U);
    EXPECT_EQ(view.height, 2U);
    EXPECT_EQ(view.rawImage.size(), 4U);
    EXPECT_EQ(view.rawImage[2], 3U);
    EXPECT_EQ(view.displayImage[3], 40U);
}

TEST(FrameView, ExposesMutableImageSpans) {
    Dss::Processing::FramePacket packet;
    packet.rawImage = {1, 2, 3};

    auto view = Dss::Processing::makeMutableFrameView(packet);
    view.rawImage[1] = 99;

    EXPECT_EQ(packet.rawImage[1], 99U);
}
