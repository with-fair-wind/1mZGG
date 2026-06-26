#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/network/image_sender.h"

namespace {

[[nodiscard]] auto readU16Be(const std::vector<uint8_t>& packet, size_t offset) -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(packet[offset]) << 8U) |
                                 packet[offset + 1U]);
}

[[nodiscard]] auto readU32Be(const std::vector<uint8_t>& packet, size_t offset) -> uint32_t {
    return (static_cast<uint32_t>(packet[offset]) << 24U) |
           (static_cast<uint32_t>(packet[offset + 1U]) << 16U) |
           (static_cast<uint32_t>(packet[offset + 2U]) << 8U) |
           static_cast<uint32_t>(packet[offset + 3U]);
}

[[nodiscard]] auto readU32Le(const std::vector<uint8_t>& packet, size_t offset) -> uint32_t {
    return static_cast<uint32_t>(packet[offset]) |
           (static_cast<uint32_t>(packet[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(packet[offset + 2U]) << 16U) |
           (static_cast<uint32_t>(packet[offset + 3U]) << 24U);
}

}  // namespace

TEST(ImageSender, BuildPacketsMatchesLegacyUdpFraming) {
    const std::vector<uint8_t> image{1, 2, 3, 4, 5};

    const auto packets = Dss::Network::ImageSender::buildPackets(image, 320, 240);

    ASSERT_EQ(packets.size(), 1U);
    const auto& packet = packets.front();
    const auto encodedLength = image.size() + Dss::Network::ImageSender::ImageHeaderSize;
    ASSERT_EQ(packet.size(), Dss::Network::ImageSender::PacketHeaderSize + encodedLength +
                                 Dss::Network::ImageSender::PacketPaddingSize);
    EXPECT_EQ(readU32Le(packet, 0), 0xAAAA5555U);
    EXPECT_EQ(readU32Le(packet, 4), 1U);
    EXPECT_EQ(readU32Le(packet, 8), encodedLength);
    EXPECT_EQ(readU32Le(packet, 12), 1U);
    EXPECT_EQ(readU32Le(packet, 16), encodedLength);
    EXPECT_EQ(readU32Be(packet, 20), encodedLength);
    EXPECT_EQ(readU16Be(packet, 24), 320U);
    EXPECT_EQ(readU16Be(packet, 26), 240U);
    EXPECT_EQ(packet[28], 1U);
    EXPECT_EQ(packet[29], 8U);
    EXPECT_TRUE(std::equal(image.begin(), image.end(),
                           packet.begin() + Dss::Network::ImageSender::PacketHeaderSize +
                               Dss::Network::ImageSender::ImageHeaderSize));
}

TEST(ImageSender, BuildPacketsFragmentsLargeImages) {
    const auto chunkSize = Dss::Network::ImageSender::MaxUdpPayload;
    std::vector<uint8_t> image(chunkSize + 3U);
    for (size_t i = 0; i < image.size(); ++i) {
        image[i] = static_cast<uint8_t>(i & 0xFFU);
    }

    const auto packets = Dss::Network::ImageSender::buildPackets(image, 6144, 6144);

    ASSERT_EQ(packets.size(), 2U);
    const auto encodedLength = image.size() + Dss::Network::ImageSender::ImageHeaderSize;
    EXPECT_EQ(readU32Le(packets[0], 4), 2U);
    EXPECT_EQ(readU32Le(packets[1], 4), 2U);
    EXPECT_EQ(readU32Le(packets[0], 8), encodedLength);
    EXPECT_EQ(readU32Le(packets[1], 8), encodedLength);
    EXPECT_EQ(readU32Le(packets[0], 12), 1U);
    EXPECT_EQ(readU32Le(packets[1], 12), 2U);
    EXPECT_EQ(readU32Le(packets[0], 16), chunkSize);
    EXPECT_EQ(readU32Le(packets[1], 16), Dss::Network::ImageSender::ImageHeaderSize + 3U);
    EXPECT_EQ(packets[1][Dss::Network::ImageSender::PacketHeaderSize],
              image[chunkSize - Dss::Network::ImageSender::ImageHeaderSize]);
}
