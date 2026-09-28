#include <chrono>
#include <future>
#include <memory>
#include <stop_token>

#include <gtest/gtest.h>

#include "dss/processing/detail/bounded_channel.h"
using namespace std::chrono_literals;

TEST(BoundedChannelTest, PreservesFifoOrder) {
    Dss::Processing::BoundedChannel<int, 2> channel;
    std::stop_source stopSource;

    EXPECT_TRUE(channel.tryPush(10));
    EXPECT_TRUE(channel.tryPush(20));
    EXPECT_FALSE(channel.tryPush(30));

    const auto first = channel.pop(stopSource.get_token());
    const auto second = channel.pop(stopSource.get_token());

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(*second, 20);
    EXPECT_TRUE(channel.empty());
}

TEST(BoundedChannelTest, PopReturnsEmptyWhenStopped) {
    Dss::Processing::BoundedChannel<int, 1> channel;
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto value = channel.pop(stopSource.get_token());

    EXPECT_FALSE(value.has_value());
}

TEST(BoundedChannelTest, BlockingPushReturnsFalseWhenStopped) {
    Dss::Processing::BoundedChannel<int, 1> channel;
    std::stop_source stopSource;

    ASSERT_TRUE(channel.tryPush(10));

    auto pushed = std::async(
        std::launch::async,
        [&channel, token = stopSource.get_token()]() mutable { return channel.push(20, token); });

    ASSERT_EQ(pushed.wait_for(50ms), std::future_status::timeout);
    stopSource.request_stop();

    EXPECT_FALSE(pushed.get());
    EXPECT_EQ(channel.size(), 1U);
}

TEST(BoundedChannelTest, ClearReleasesPendingPayloadsAndChannelCanBeReused) {
    Dss::Processing::BoundedChannel<std::shared_ptr<int>, 2> channel;
    auto payload = std::make_shared<int>(3);
    std::weak_ptr<int> observer = payload;
    ASSERT_TRUE(channel.tryPush(std::move(payload)));
    channel.clear();
    EXPECT_TRUE(observer.expired());
    EXPECT_TRUE(channel.empty());
    ASSERT_TRUE(channel.tryPush(std::make_shared<int>(4)));
    auto next = channel.tryPop();
    ASSERT_TRUE(next);
    EXPECT_EQ(**next, 4);
}

TEST(BoundedChannel, ResourceBytesFollowPopClearAndReopen) {
    Dss::Processing::BoundedChannel<int, 2> channel;
    EXPECT_TRUE(channel.tryPush(1, 8));
    EXPECT_TRUE(channel.tryPush(2, 12));
    EXPECT_FALSE(channel.tryPush(3, 100));
    EXPECT_EQ(channel.resourceSnapshot().queuedBytes, 20U);
    EXPECT_EQ(channel.resourceSnapshot().peakQueuedBytes, 20U);
    EXPECT_EQ(channel.tryPop(), 1);
    EXPECT_EQ(channel.resourceSnapshot().queuedBytes, 12U);
    channel.close();
    channel.clear();
    channel.open();
    EXPECT_TRUE(channel.push(4, {}, 3));
    EXPECT_EQ(channel.resourceSnapshot().queuedBytes, 3U);
    EXPECT_EQ(channel.resourceSnapshot().peakQueuedBytes, 20U);
    EXPECT_EQ(channel.pop({}), 4);
    EXPECT_EQ(channel.resourceSnapshot().queuedBytes, 0U);
}
