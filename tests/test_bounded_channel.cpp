#include "dss/processing/bounded_channel.h"

#include <gtest/gtest.h>

#include <stop_token>

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
