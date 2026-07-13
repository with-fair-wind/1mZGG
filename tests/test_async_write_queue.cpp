#include <chrono>
#include <latch>
#include <thread>

#include <gtest/gtest.h>

#include "dss/storage/detail/async_write_queue.h"

using namespace std::chrono_literals;

TEST(AsyncWriteQueue, StopsAcceptingBeforeDrainingAcceptedRequests) {
    Dss::Storage::AsyncWriteQueue<int> queue;
    std::latch writerEntered{1};
    std::latch releaseWriter{1};
    ASSERT_TRUE(queue
                    .start([&](const int& request) -> std::expected<void, std::string> {
                        if (request == 1) {
                            writerEntered.count_down();
                            releaseWriter.wait();
                        }
                        return {};
                    })
                    .has_value());
    ASSERT_TRUE(queue.enqueue(1).has_value());
    writerEntered.wait();

    std::jthread stopper([&queue] { queue.stop(); });
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (queue.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const auto stoppedAccepting = !queue.isRunning();
    const auto enqueueWhileStopping = queue.enqueue(2);

    releaseWriter.count_down();
    stopper.join();

    ASSERT_TRUE(stoppedAccepting);
    EXPECT_FALSE(enqueueWhileStopping.has_value());
    EXPECT_EQ(queue.successfulWrites(), 1U);
}
