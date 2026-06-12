#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/result_packet_utils.h"

namespace {

[[nodiscard]] auto makeFrame(std::uint64_t frameSeq, bool valid) -> Dss::Core::TargetFrameInfo {
    Dss::Core::MeasuredBlob blob{};
    blob.id = "blob-fallback";
    blob.centroid = Dss::Core::Vec2f{123.5F, 456.25F};
    blob.area = 12.0F;
    blob.posAe = Dss::Core::Vec2f{1.5F, 2.5F};
    blob.distAzi = -0.25F;
    blob.distEle = 0.5F;
    blob.fovCenterAziModify = 9.5F;
    blob.fovCenterEleModify = 8.5F;

    Dss::Core::TargetFrameInfo frame{};
    frame.timestamp = {
        .year = 2026, .month = 6, .day = 12, .hour = 1, .minute = 2, .second = 3, .millisecond = 4};
    frame.frameSeq = frameSeq;
    frame.fovCenterAe = Dss::Core::Vec2f{7.0F, 8.0F};
    frame.opticCenter = Dss::Core::Vec2f{3072.0F, 3073.0F};
    frame.exposureTime = 0.02F;
    frame.frameFreq = 50.0F;
    frame.measuredBlob = blob;
    frame.posZxdw = Dss::Core::Vec2f{3.0F, 4.0F};
    frame.posTwdw = Dss::Core::Vec2f{5.0F, 6.0F};
    frame.magnitude = 11.25F;
    frame.valid = valid;
    return frame;
}

}  // namespace

TEST(ResultPacketUtils, BuildsPacketFromLatestTargetFrame) {
    Dss::Core::TargetInfo target{};
    target.targetId = "target-42";
    target.predictedSpdAe = Dss::Core::Vec2f{0.125F, -0.25F};
    target.frameInfos.push_back(makeFrame(100U, true));
    target.frameInfos.push_back(makeFrame(101U, false));

    const auto packet = Dss::Core::makeResultPacket(target);

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->targetId, "target-42");
    EXPECT_EQ(packet->frameSeq, 101U);
    EXPECT_EQ(packet->timestamp.year, 2026);
    EXPECT_FLOAT_EQ(packet->exposureTime, 0.02F);
    EXPECT_FLOAT_EQ(packet->frameFreq, 50.0F);
    EXPECT_FLOAT_EQ(packet->fovCenterAe.x, 7.0F);
    EXPECT_FLOAT_EQ(packet->fovCenterAeModified.x, 9.5F);
    EXPECT_FLOAT_EQ(packet->opticCenter.y, 3073.0F);
    EXPECT_FLOAT_EQ(packet->targetPosFrame.x, 123.5F);
    EXPECT_FLOAT_EQ(packet->targetDistAe.y, 0.5F);
    EXPECT_FLOAT_EQ(packet->targetSpdAe.x, 0.125F);
    EXPECT_FLOAT_EQ(packet->targetSpdAe.y, -0.25F);
    EXPECT_FLOAT_EQ(packet->targetPosZxdw.x, 3.0F);
    EXPECT_FLOAT_EQ(packet->targetPosTwdw.y, 6.0F);
    EXPECT_FLOAT_EQ(packet->targetMvGdcl, 11.25F);
    EXPECT_FALSE(packet->valid);
}

TEST(ResultPacketUtils, FallsBackToBlobIdWhenTargetIdIsEmpty) {
    Dss::Core::TargetInfo target{};
    target.frameInfos.push_back(makeFrame(7U, true));

    const auto packet = Dss::Core::makeResultPacket(target);

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->targetId, "blob-fallback");
}

TEST(ResultPacketUtils, SkipsTargetsWithoutFrameInfo) {
    Dss::Core::TargetInfo emptyTarget{};
    Dss::Core::TargetInfo validTarget{};
    validTarget.targetId = "valid";
    validTarget.frameInfos.push_back(makeFrame(9U, true));
    const std::vector<Dss::Core::TargetInfo> targets{emptyTarget, validTarget};

    EXPECT_EQ(Dss::Core::latestTargetFrameInfo(emptyTarget), nullptr);
    const auto packets = Dss::Core::makeResultPackets(targets);

    ASSERT_EQ(packets.size(), 1U);
    EXPECT_EQ(packets.front().targetId, "valid");
}
