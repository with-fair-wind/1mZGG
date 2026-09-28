#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/network/transport/udp_channel.h"

namespace {
using namespace std::chrono_literals;
using Channel = Dss::Network::UdpChannel;
const Dss::Network::UdpEndpointConfig local{
    .localIp = "127.0.0.1", .localPort = 0, .remoteIp = {}, .remotePort = 0};

template <class Predicate>
bool waitUntil(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

// Shared by the callback and owner: timeout also releases workers on an early test failure.
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, released = false;
    void enter() {
        std::unique_lock lock(mutex);
        entered = true;
        cv.notify_all();
        cv.wait_for(lock, 5s, [&] { return released; });
    }
    bool waitEntered() {
        std::unique_lock lock(mutex);
        return cv.wait_for(lock, 3s, [&] { return entered; });
    }
    void release() {
        std::lock_guard lock(mutex);
        released = true;
        cv.notify_all();
    }
};
}  // namespace

TEST(UdpSendQueue, EnforcesCountAndByteLimitsAndDrainsAcceptedRequestsOnClose) {
    for (const auto byteLimit : {8U, 100U}) {
        Channel sender(2, byteLimit), trigger, receiver;
        auto gate = std::make_shared<Gate>();
        sender.setReceiveCallback([gate](auto, const auto&, auto) { gate->enter(); });
        ASSERT_TRUE(sender.bind(local));
        ASSERT_TRUE(trigger.bind(local));
        ASSERT_TRUE(receiver.bind(local));
        const std::vector<std::uint8_t> packet{1, 2, 3, 4};
        ASSERT_EQ(trigger.sendTo(packet, "127.0.0.1", sender.localPort()), 4);
        ASSERT_TRUE(gate->waitEntered());
        auto send = [&] { return sender.sendTo(packet, "127.0.0.1", receiver.localPort()); };
        auto first = std::async(std::launch::async, send);
        ASSERT_TRUE(waitUntil([&] { return sender.sendQueueSnapshot().pendingRequests == 1; }));
        if (byteLimit == 8) {
            const std::vector<std::uint8_t> tooManyBytes(5);
            EXPECT_EQ(sender.sendTo(tooManyBytes, "127.0.0.1", receiver.localPort()), -1);
        }
        auto second = std::async(std::launch::async, send);
        ASSERT_TRUE(waitUntil([&] { return sender.sendQueueSnapshot().pendingRequests == 2; }));
        EXPECT_EQ(send(), -1);
        EXPECT_EQ(sender.sendQueueSnapshot().pendingBytes, 8U);
        auto close = std::async(std::launch::async, [&] { sender.close(); });
        EXPECT_TRUE(waitUntil([&] { return !sender.sendQueueSnapshot().accepting; }));
        EXPECT_EQ(send(), -1);
        gate->release();
        ASSERT_EQ(close.wait_for(3s), std::future_status::ready);
        close.get();
        EXPECT_EQ(first.get(), 4);
        EXPECT_EQ(second.get(), 4);
        EXPECT_EQ(sender.sendQueueSnapshot().pendingRequests, 0U);
        EXPECT_EQ(sender.sendQueueSnapshot().pendingBytes, 0U);
        EXPECT_GE(sender.sendQueueSnapshot().rejectedRequests, 2U);
        EXPECT_FALSE(sender.isBound());
        ASSERT_TRUE(sender.bind(local));
        EXPECT_EQ(send(), 4);
    }
}

TEST(UdpSendQueue, RejectsOversizedDatagramsBeforeQueueing) {
    Channel sender;
    ASSERT_TRUE(sender.bind(local));
    const std::vector<std::uint8_t> packet(Channel::maxDatagramBytes + 1);
    EXPECT_EQ(sender.sendTo(packet, "127.0.0.1", 12345), -1);
    EXPECT_EQ(sender.sendQueueSnapshot().pendingBytes, 0U);
    EXPECT_EQ(sender.sendQueueSnapshot().rejectedRequests, 1U);
}

TEST(UdpSendQueue, CallbackCanSendReplyWithoutWaitingForItsOwnWorker) {
    Channel sender, receiver;
    auto reply = std::make_shared<std::promise<std::vector<std::uint8_t>>>();
    auto result = reply->get_future();
    sender.setReceiveCallback(
        [reply](auto bytes, const auto&, auto) { reply->set_value({bytes.begin(), bytes.end()}); });
    receiver.setReceiveCallback([&](auto bytes, const auto& host, auto port) {
        EXPECT_EQ(receiver.sendTo(bytes, host, port), static_cast<std::int64_t>(bytes.size()));
    });
    ASSERT_TRUE(sender.bind(local));
    ASSERT_TRUE(receiver.bind(local));
    const std::vector<std::uint8_t> packet{0, 255, 42};
    ASSERT_EQ(sender.sendTo(packet, "127.0.0.1", receiver.localPort()), 3);
    ASSERT_EQ(result.wait_for(3s), std::future_status::ready);
    EXPECT_EQ(result.get(), packet);
    sender.close();
    receiver.close();
}

TEST(UdpSendQueue, ThrowingCallbackFailsPendingSendAndAllowsRebind) {
    Channel sender, trigger;
    auto gate = std::make_shared<Gate>();
    sender.setReceiveCallback([gate](auto, const auto&, auto) {
        gate->enter();
        throw std::runtime_error("callback failure");
    });
    ASSERT_TRUE(sender.bind(local));
    ASSERT_TRUE(trigger.bind(local));
    const std::vector<std::uint8_t> packet{1};
    ASSERT_EQ(trigger.sendTo(packet, "127.0.0.1", sender.localPort()), 1);
    ASSERT_TRUE(gate->waitEntered());
    auto pending = std::async(std::launch::async, [&] {
        return sender.sendTo(packet, "127.0.0.1", trigger.localPort());
    });
    ASSERT_TRUE(waitUntil([&] { return sender.sendQueueSnapshot().pendingRequests == 1; }));
    gate->release();
    ASSERT_EQ(pending.wait_for(3s), std::future_status::ready);
    EXPECT_EQ(pending.get(), -1);
    ASSERT_TRUE(waitUntil([&] { return !sender.isBound(); }));
    sender.close();
    sender.setReceiveCallback({});
    ASSERT_TRUE(sender.bind(local));
    EXPECT_EQ(sender.sendTo(packet, "127.0.0.1", trigger.localPort()), 1);
}

TEST(UdpSendQueue, RepeatedIdleCloseAndRebindCompletes) {
    Channel channel;
    for (int iteration = 0; iteration < 30; ++iteration) {
        ASSERT_TRUE(channel.bind(local));
        channel.close();
        EXPECT_FALSE(channel.isBound());
        EXPECT_FALSE(channel.sendQueueSnapshot().accepting);
    }
}
