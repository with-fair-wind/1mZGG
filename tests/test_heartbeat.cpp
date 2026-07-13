#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include "dss/core/concurrency/interruptible_wait.h"
#include "dss/core/constants.h"
#include "dss/network/endpoint/heartbeat.h"

TEST(Heartbeat, BuildsHeartbeatFrame) {
    const auto frame = Dss::Network::Heartbeat::buildFrame();

    ASSERT_EQ(frame.size(), 10U);
    EXPECT_EQ(frame.front(), Dss::Core::FrameHeader);
    EXPECT_EQ(frame.back(), Dss::Core::FrameTail);
    EXPECT_EQ(frame[1], 0x00);
}

TEST(Heartbeat, BuildsCloseGuardFrame) {
    const auto frame = Dss::Network::Heartbeat::buildCloseGuardFrame();

    ASSERT_EQ(frame.size(), 10U);
    EXPECT_EQ(frame.front(), Dss::Core::FrameHeader);
    EXPECT_EQ(frame.back(), Dss::Core::FrameTail);
    EXPECT_EQ(frame[1], 0x01);
}

TEST(InterruptibleWait, StopRequestEndsLongWaitPromptly) {
    using namespace std::chrono_literals;
    Dss::Core::InterruptibleWait wait;
    std::promise<void> started;
    auto ready = started.get_future();
    std::jthread worker([&wait, &started](std::stop_token token) {
        started.set_value();
        static_cast<void>(wait.waitFor(token, 10s));
    });
    ready.wait();

    const auto begin = std::chrono::steady_clock::now();
    worker.request_stop();
    worker.join();

    EXPECT_LT(std::chrono::steady_clock::now() - begin, 1s);
}
