#include "dss/processing/bounded_channel.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <stop_token>

using namespace std::chrono_literals;

TEST(BoundedChannelTest, PreservesFifoOrder)
{
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

TEST(BoundedChannelTest, PopReturnsEmptyWhenStopped)
{
    Dss::Processing::BoundedChannel<int, 1> channel;
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto value = channel.pop(stopSource.get_token());

    EXPECT_FALSE(value.has_value());
}

TEST(BoundedChannelTest, BlockingPushReturnsFalseWhenStopped)
{
    Dss::Processing::BoundedChannel<int, 1> channel;
    std::stop_source stopSource;

    ASSERT_TRUE(channel.tryPush(10));

    auto pushed = std::async(std::launch::async, [&channel, token = stopSource.get_token()]() mutable {
        return channel.push(20, token);
    });

    ASSERT_EQ(pushed.wait_for(50ms), std::future_status::timeout);
    stopSource.request_stop();

    EXPECT_FALSE(pushed.get());
    EXPECT_EQ(channel.size(), 1U);
}
