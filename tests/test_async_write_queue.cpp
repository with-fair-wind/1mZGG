#include <chrono>
#include <latch>
#include <stdexcept>
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

TEST(AsyncWriteQueue, WriterAndErrorCallbackExceptionsDoNotAbortDraining) {
    Dss::Storage::AsyncWriteQueue<int> queue;
    int notified = 0;
    ASSERT_TRUE(queue.start(
        [](const int& value) -> std::expected<void, std::string> {
            if (value == 1)
                throw std::runtime_error("injected writer failure");
            return {};
        },
        [&](const int&, const std::string&) {
            ++notified;
            throw std::runtime_error("injected diagnostic failure");
        }));
    ASSERT_TRUE(queue.enqueue(1));
    ASSERT_TRUE(queue.enqueue(2));
    queue.stop();
    EXPECT_EQ(queue.failedWrites(), 1U);
    EXPECT_EQ(queue.successfulWrites(), 1U);
    EXPECT_EQ(notified, 1);
    ASSERT_TRUE(queue.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    ASSERT_TRUE(queue.enqueue(3));
    queue.stop();
    EXPECT_EQ(queue.successfulWrites(), 2U);
}

TEST(AsyncWriteQueue, ByteBudgetIncludesActiveWriterAndRequestStopDoesNotWait) {
    Dss::Storage::AsyncWriteQueue<int> queue(10, 8);
    std::latch entered{1};
    std::latch release{1};
    ASSERT_TRUE(queue.start([&](const int&) -> std::expected<void, std::string> {
        entered.count_down();
        release.wait();
        return {};
    }));
    ASSERT_TRUE(queue.enqueue(1, 6));
    entered.wait();
    EXPECT_EQ(queue.pendingBytes(), 6U);
    EXPECT_FALSE(queue.enqueue(2, 3));
    queue.requestStop();
    EXPECT_FALSE(queue.isRunning());
    EXPECT_FALSE(queue.enqueue(3, 1));
    release.count_down();
    queue.stop();
    EXPECT_EQ(queue.pendingBytes(), 0U);
    EXPECT_EQ(queue.droppedRequests(), 1U);
}

TEST(AsyncWriteQueue, SnapshotSeparatesActiveAndQueuedPayloadsAcrossRestart) {
    Dss::Storage::AsyncWriteQueue<int> queue(4, 20);
    std::latch entered{1};
    std::latch release{1};
    ASSERT_TRUE(queue.start([&](const int& value) -> std::expected<void, std::string> {
        if (value == 1) {
            entered.count_down();
            release.wait();
        }
        return {};
    }));
    ASSERT_TRUE(queue.enqueue(1, 8));
    entered.wait();
    const auto queued = queue.enqueue(2, 12);
    const auto blocked = queue.resourceSnapshot();
    const auto rejected = queue.enqueue(3, 1);
    release.count_down();
    queue.stop();
    EXPECT_TRUE(queued);
    EXPECT_FALSE(rejected);
    EXPECT_EQ(blocked.activeBytes, 8U);
    EXPECT_EQ(blocked.queuedBytes, 12U);
    EXPECT_EQ(blocked.queuedItems, 1U);
    EXPECT_EQ(queue.resourceSnapshot().activeBytes, 0U);
    EXPECT_EQ(queue.resourceSnapshot().queuedBytes, 0U);
    EXPECT_EQ(queue.resourceSnapshot().completedItems, 2U);
    ASSERT_TRUE(queue.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    ASSERT_TRUE(queue.enqueue(4, 1));
    queue.stop();
    EXPECT_EQ(queue.resourceSnapshot().completedItems, 3U);
    EXPECT_EQ(queue.resourceSnapshot().peakQueuedBytes, 12U);
}
