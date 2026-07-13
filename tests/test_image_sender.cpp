#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/network/endpoint/image_sender.h"
#include "dss/network/transport/udp_channel.h"

using namespace std::chrono_literals;

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

TEST(UdpChannel, SendsAndReceivesAcrossCallerThreads) {
    Dss::Network::UdpChannel receiver;
    std::promise<std::vector<uint8_t>> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();
    receiver.setReceiveCallback(
        [&receivedPromise](std::span<const uint8_t> data, const std::string&, uint16_t) {
            receivedPromise.set_value({data.begin(), data.end()});
        });

    ASSERT_TRUE(
        receiver.bind({.localIp = "127.0.0.1", .localPort = 0, .remoteIp = {}, .remotePort = 0})
            .has_value());
    ASSERT_NE(receiver.localPort(), 0U);

    Dss::Network::UdpChannel sender;
    ASSERT_TRUE(sender
                    .bind({.localIp = "127.0.0.1",
                           .localPort = 0,
                           .remoteIp = "127.0.0.1",
                           .remotePort = receiver.localPort()})
                    .has_value());

    const std::vector<uint8_t> payload{4, 3, 2, 1};
    std::promise<int64_t> sentPromise;
    auto sentFuture = sentPromise.get_future();
    std::jthread caller(
        [&sender, &payload, &sentPromise] { sentPromise.set_value(sender.send(payload)); });

    ASSERT_EQ(sentFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(sentFuture.get(), static_cast<int64_t>(payload.size()));
    ASSERT_EQ(receivedFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(receivedFuture.get(), payload);
}

TEST(ImageSender, PublishesCompletedEventWithSubmittedFrameSequence) {
    Dss::Network::ImageSender::MessageBus bus;
    Dss::Network::ImageSender sender(bus);
    std::promise<Dss::Core::ImageSendCompletedEvent> completedPromise;
    auto completedFuture = completedPromise.get_future();
    auto connection = bus.subscribe<Dss::Core::ImageSendCompletedEvent>(
        [&completedPromise](const auto& event) { completedPromise.set_value(event); });

    ASSERT_TRUE(
        sender
            .open(
                {.localIp = "127.0.0.1", .localPort = 0, .remoteIp = "127.0.0.1", .remotePort = 9})
            .has_value());
    auto image = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1, 2, 3, 4});

    sender.sendImage(77U, image, 2U, 2U);

    ASSERT_EQ(completedFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(completedFuture.get().frameSeq, 77U);
    sender.close();
}
