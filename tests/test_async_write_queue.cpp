#include <chrono>
#include <future>
#include <latch>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>

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

namespace {
/// @brief 断言提前退出时仍释放 writer；须晚于队列构造。
struct ReleaseStorageLatch {
    explicit ReleaseStorageLatch(std::latch& latch) : value(latch) {}
    ReleaseStorageLatch(const ReleaseStorageLatch&) = delete;
    auto operator=(const ReleaseStorageLatch&) -> ReleaseStorageLatch& = delete;
    ReleaseStorageLatch(ReleaseStorageLatch&&) = delete;
    auto operator=(ReleaseStorageLatch&&) -> ReleaseStorageLatch& = delete;
    ~ReleaseStorageLatch() {
        if (!value.try_wait()) {
            value.count_down();
        }
    }
    std::latch& value;
};
/// @brief 通过队列内可观测等待数建立交错，不以固定睡眠代替同步。
auto waitForStorageWaiter(Dss::Storage::AsyncWriteQueue<int>& queue) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (queue.resourceSnapshot().waitingItems != 0) {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}
}  // namespace

TEST(AsyncWriteQueue, BlockingAdmissionWaitsForCapacityAndKeepsAcceptedOrder) {
    std::latch firstEntered{1};
    std::latch releaseFirst{1};
    std::latch secondEntered{1};
    std::latch releaseSecond{1};
    std::vector<int> written;
    Dss::Storage::AsyncWriteQueue<int> queue(1);
    const ReleaseStorageLatch releaseFirstOnExit(releaseFirst);
    const ReleaseStorageLatch releaseSecondOnExit(releaseSecond);
    ASSERT_TRUE(queue.start([&](const int& value) -> std::expected<void, std::string> {
        if (value == 1) {
            firstEntered.count_down();
            releaseFirst.wait();
        } else if (value == 2) {
            secondEntered.count_down();
            releaseSecond.wait();
        }
        written.push_back(value);
        return {};
    }));
    ASSERT_TRUE(queue.enqueue(1));
    firstEntered.wait();
    ASSERT_TRUE(queue.enqueue(2));
    const auto generation = queue.generation();
    auto producer =
        std::async(std::launch::async, [&] { return queue.enqueueBlocking(3, 0, {}, generation); });
    const auto waited = waitForStorageWaiter(queue);
    releaseFirst.count_down();
    secondEntered.wait();
    // 第二项仍在写时，出队已释放请求数槽位，第三项必须能入队。
    const auto ready = producer.wait_for(2s);
    if (ready != std::future_status::ready) {
        queue.requestStop();
    }
    releaseSecond.count_down();
    queue.stop();
    const auto result = producer.get();
    EXPECT_TRUE(waited);
    EXPECT_EQ(ready, std::future_status::ready);
    EXPECT_TRUE(result);
    EXPECT_EQ(written, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(queue.droppedRequests(), 0U);
    EXPECT_EQ(queue.resourceSnapshot().waitingItems, 0U);
}

TEST(AsyncWriteQueue, CancellationWakesByteBudgetWaiterWithoutDroppingAcceptedWrite) {
    std::latch entered{1};
    std::latch release{1};
    Dss::Storage::AsyncWriteQueue<int> queue(4, 8);
    const ReleaseStorageLatch releaseOnExit(release);
    ASSERT_TRUE(queue.start([&](const int&) -> std::expected<void, std::string> {
        entered.count_down();
        release.wait();
        return {};
    }));
    ASSERT_TRUE(queue.enqueue(1, 6));
    entered.wait();
    std::stop_source cancellation;
    const auto generation = queue.generation();
    auto producer = std::async(std::launch::async, [&] {
        return queue.enqueueBlocking(2, 3, cancellation.get_token(), generation);
    });
    const auto waited = waitForStorageWaiter(queue);
    cancellation.request_stop();
    const auto ready = producer.wait_for(2s);
    if (ready != std::future_status::ready) {
        queue.requestStop();
    }
    const auto retained = queue.pendingBytes();
    release.count_down();
    queue.stop();
    const auto result = producer.get();
    EXPECT_TRUE(waited);
    EXPECT_EQ(ready, std::future_status::ready);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Dss::Storage::WriteAdmissionError::Cancelled);
    EXPECT_EQ(retained, 6U);
    EXPECT_EQ(queue.successfulWrites(), 1U);
    EXPECT_EQ(queue.droppedRequests(), 0U);
    EXPECT_EQ(queue.pendingBytes(), 0U);
}

TEST(AsyncWriteQueue, StopWakesWaiterAndRestartRejectsOldAdmissionGeneration) {
    std::latch entered{1};
    std::latch release{1};
    Dss::Storage::AsyncWriteQueue<int> queue(4, 8);
    const ReleaseStorageLatch releaseOnExit(release);
    ASSERT_TRUE(queue.start([&](const int&) -> std::expected<void, std::string> {
        entered.count_down();
        release.wait();
        return {};
    }));
    ASSERT_TRUE(queue.enqueue(1, 8));
    entered.wait();
    const auto oldGeneration = queue.generation();
    auto producer = std::async(std::launch::async,
                               [&] { return queue.enqueueBlocking(2, 1, {}, oldGeneration); });
    const auto waited = waitForStorageWaiter(queue);
    queue.requestStop();
    const auto ready = producer.wait_for(2s);
    release.count_down();
    queue.stop();
    const auto result = producer.get();
    EXPECT_TRUE(waited);
    EXPECT_EQ(ready, std::future_status::ready);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Dss::Storage::WriteAdmissionError::Stopped);
    ASSERT_TRUE(queue.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    const auto stale = queue.enqueueBlocking(3, 1, {}, oldGeneration);
    EXPECT_FALSE(stale);
    if (!stale) {
        EXPECT_EQ(stale.error(), Dss::Storage::WriteAdmissionError::StaleSession);
    }
    EXPECT_TRUE(queue.enqueueBlocking(4, 1, {}, queue.generation()));
    queue.stop();
    EXPECT_EQ(queue.successfulWrites(), 2U);
    EXPECT_EQ(queue.droppedRequests(), 0U);
}

TEST(AsyncWriteQueue, WriterFailureWakesLosslessProducerAndCompletionBarrier) {
    std::latch entered{1};
    std::latch release{1};
    Dss::Storage::AsyncWriteQueue<int> queue(4, 8);
    const ReleaseStorageLatch releaseOnExit(release);
    ASSERT_TRUE(queue.start([&](const int&) -> std::expected<void, std::string> {
        entered.count_down();
        release.wait();
        return std::unexpected("injected disk failure");
    }));
    ASSERT_TRUE(queue.enqueue(1, 8));
    entered.wait();
    const auto generation = queue.generation();
    auto producer =
        std::async(std::launch::async, [&] { return queue.enqueueBlocking(2, 1, {}, generation); });
    const auto waited = waitForStorageWaiter(queue);
    release.count_down();
    const auto ready = producer.wait_for(2s);
    if (ready != std::future_status::ready) {
        queue.requestStop();
    }
    const auto result = producer.get();
    const auto barrier = queue.waitUntilIdle({});
    queue.stop();
    EXPECT_TRUE(waited);
    EXPECT_EQ(ready, std::future_status::ready);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Dss::Storage::WriteAdmissionError::WriterFailed);
    ASSERT_FALSE(barrier);
    EXPECT_EQ(barrier.error(), Dss::Storage::WriteAdmissionError::WriterFailed);
    EXPECT_EQ(queue.failedWrites(), 1U);
    EXPECT_EQ(queue.resourceSnapshot().completedItems, 1U);
    EXPECT_EQ(queue.droppedRequests(), 0U);
    ASSERT_TRUE(queue.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    ASSERT_TRUE(queue.enqueueBlocking(3, 1, {}, queue.generation()));
    EXPECT_TRUE(queue.waitUntilIdle({}));
    queue.stop();
    EXPECT_EQ(queue.successfulWrites(), 1U);
}

TEST(AsyncWriteQueue, ImpossibleLosslessRequestsRejectImmediatelyWithoutWaiting) {
    Dss::Storage::AsyncWriteQueue<int> queue(1, 8);
    ASSERT_TRUE(queue.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    const auto oversized = queue.enqueueBlocking(1, 9, {}, queue.generation());
    ASSERT_FALSE(oversized);
    EXPECT_EQ(oversized.error(), Dss::Storage::WriteAdmissionError::CapacityExceeded);
    EXPECT_EQ(queue.resourceSnapshot().waitingItems, 0U);
    EXPECT_EQ(queue.droppedRequests(), 1U);
    std::stop_source cancelled;
    cancelled.request_stop();
    const auto cancellation =
        queue.enqueueBlocking(2, 1, cancelled.get_token(), queue.generation());
    ASSERT_FALSE(cancellation);
    EXPECT_EQ(cancellation.error(), Dss::Storage::WriteAdmissionError::Cancelled);
    queue.stop();
    Dss::Storage::AsyncWriteQueue<int> disabled(0);
    ASSERT_TRUE(disabled.start([](const int&) -> std::expected<void, std::string> { return {}; }));
    const auto rejected = disabled.enqueueBlocking(3, 0, {}, disabled.generation());
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), Dss::Storage::WriteAdmissionError::CapacityExceeded);
    disabled.stop();
}
