#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/app/replay_session.h"
#include "dss/core/event/events.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/storage/format/image_storage_format.h"

namespace {
using namespace std::chrono_literals;
using Session = Dss::App::ReplaySession;
using State = Session::State;

template <class Predicate>
bool waitUntil(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

class Gate {
public:
    void enter() {
        std::unique_lock lock(m_mutex);
        m_entered = true;
        m_cv.notify_all();
        m_cv.wait_for(lock, 5s, [this] { return m_released; });
    }
    bool waitEntered() {
        std::unique_lock lock(m_mutex);
        return m_cv.wait_for(lock, 3s, [this] { return m_entered; });
    }
    void release() {
        std::lock_guard lock(m_mutex);
        m_released = true;
        m_cv.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_entered = false;
    bool m_released = false;
};

struct ReleaseGate {
    Gate& gate;
    ~ReleaseGate() {
        gate.release();
    }
};

class GatedProcessing final : public Dss::Processing::IProcessingStrategy {
public:
    explicit GatedProcessing(std::shared_ptr<Gate> gate) : m_gate(std::move(gate)) {}
    auto process(const Dss::Processing::FramePacket&)
        -> Dss::Processing::ProcessingResult override {
        m_gate->enter();
        Dss::Processing::ProcessingResult result;
        result.success = true;
        return result;
    }
    auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }
    auto name() const -> std::string_view override {
        return "gated";
    }

private:
    std::shared_ptr<Gate> m_gate;
};

// Uses RAW I/O and ordinary C++ synchronization: no Q(Core)Application or Qt event pumping.
class ReplayApplicationSession : public ::testing::Test {
protected:
    Dss::Core::MessageBus bus;
    std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource> source =
        std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    std::shared_ptr<Dss::Processing::ImageProcessor> processor =
        std::make_shared<Dss::Processing::ImageProcessor>(bus);
    std::unique_ptr<Session> session;
    std::filesystem::path directory;
    std::atomic<int> started{0}, stopped{0}, resets{0}, processed{0};
    std::vector<Dss::Evt::ScopedConnection> connections;

    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
                    ("dss_replay_session_" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        source->setFrameInterval(0ms);
        source->setFrameCallback([this](auto packet, auto context) {
            return processor->submitFrameBlocking(std::move(packet), context.stopToken);
        });
        connections.push_back(
            bus.subscribe<Dss::Core::GrabStartedEvent>([this](const auto&) { ++started; }));
        connections.push_back(
            bus.subscribe<Dss::Core::GrabStoppedEvent>([this](const auto&) { ++stopped; }));
        connections.push_back(bus.subscribe<Dss::Core::ProcessingSessionResetEvent>(
            [this](const auto&) { ++resets; }));
        connections.push_back(bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
            [this](const auto&) { ++processed; }));
        session = std::make_unique<Session>(bus, source, processor);
    }
    void TearDown() override {
        session->shutdown();
        session.reset();
        connections.clear();
        std::filesystem::remove_all(directory);
    }
    auto frame(const char* name) -> std::filesystem::path {
        auto path = directory / name;
        const std::vector<std::uint16_t> pixels{10, 20, 30, 40};
        const auto bytes = Dss::Storage::buildRawImageFile({.width = 2, .height = 2}, pixels);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return path;
    }
    bool select(std::vector<std::filesystem::path> files) {
        return session->selectFiles(std::move(files)).has_value() &&
               waitUntil([&] { return session->snapshot().state == State::Idle; });
    }
    bool settled() {
        return waitUntil([&] { return !session->snapshot().busy(); });
    }
};
}  // namespace

TEST_F(ReplayApplicationSession, NaturalEofDrainsAndRestartRewinds) {
    ASSERT_TRUE(select({frame("a.raw"), frame("b.raw")}));
    for (int pass = 1; pass <= 3; ++pass) {
        ASSERT_TRUE(session->start());
        ASSERT_TRUE(waitUntil([&] { return session->snapshot().state == State::Completed; }));
        EXPECT_EQ(processed.load(), pass * 2);
        EXPECT_EQ(started.load(), pass);
        EXPECT_EQ(stopped.load(), pass);
        EXPECT_FALSE(source->isRunning());
        EXPECT_FALSE(processor->isRunning());
    }
    EXPECT_EQ(resets.load(), 3);
}

TEST_F(ReplayApplicationSession, PauseRetainsHistoryAcrossRepeatedStarts) {
    source->setFrameInterval(2s);
    const auto path = frame("a.raw");
    ASSERT_TRUE(select(std::vector<std::filesystem::path>(8, path)));
    for (int pass = 1; pass <= 3; ++pass) {
        ASSERT_TRUE(session->start());
        ASSERT_TRUE(waitUntil([&] { return processed.load() >= pass; }));
        session->stop();
        ASSERT_TRUE(settled());
        EXPECT_EQ(session->snapshot().state, State::Idle);
        EXPECT_EQ(source->nextFrameIndex(), static_cast<std::size_t>(pass));
        EXPECT_EQ(resets.load(), 1);
        EXPECT_EQ(started.load(), pass);
        EXPECT_EQ(stopped.load(), pass);
    }
}

TEST_F(ReplayApplicationSession, SeekBackwardAndReplacementResetHistories) {
    ASSERT_TRUE(select({frame("a.raw"), frame("b.raw"), frame("c.raw")}));
    ASSERT_TRUE(session->step());
    ASSERT_TRUE(settled());
    ASSERT_TRUE(session->step());
    ASSERT_TRUE(settled());
    EXPECT_EQ(resets.load(), 1);
    ASSERT_TRUE(session->step(true));
    ASSERT_TRUE(settled());
    EXPECT_EQ(source->nextFrameIndex(), 1U);
    EXPECT_EQ(resets.load(), 2);
    ASSERT_TRUE(session->seek(2));
    ASSERT_TRUE(settled());
    EXPECT_EQ(source->nextFrameIndex(), 2U);
    EXPECT_EQ(resets.load(), 3);
    ASSERT_TRUE(select({frame("new.raw")}));
    EXPECT_EQ(source->nextFrameIndex(), 0U);
    EXPECT_EQ(resets.load(), 4);
    EXPECT_EQ(session->snapshot().frameCount, 1U);
    EXPECT_FALSE(session->seek(1));
}

TEST_F(ReplayApplicationSession, StopDuringRecoveryLoadingCannotStartPlayback) {
    const auto missing = directory / "missing.raw";
    ASSERT_TRUE(select({missing}));
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(settled());
    ASSERT_EQ(session->snapshot().state, State::Failed);
    (void)frame("missing.raw");
    auto sharedGate = std::make_shared<Gate>();
    auto& gate = *sharedGate;
    auto connection = bus.subscribe<Dss::Core::ProcessingSessionResetEvent>(
        [sharedGate](const auto&) { sharedGate->enter(); });
    ReleaseGate release{gate};
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(gate.waitEntered());
    EXPECT_EQ(session->snapshot().state, State::Loading);
    session->stop();
    EXPECT_EQ(session->snapshot().state, State::Stopping);
    EXPECT_FALSE(session->start());
    EXPECT_FALSE(session->selectFiles({missing}));
    gate.release();
    ASSERT_TRUE(settled());
    EXPECT_EQ(started.load(), 0);
    EXPECT_EQ(processed.load(), 0);
    EXPECT_FALSE(source->isRunning());
    EXPECT_FALSE(processor->isRunning());
}

TEST_F(ReplayApplicationSession, ShutdownWaitsForLoadingAndPermanentlyRejectsCommands) {
    auto sharedGate = std::make_shared<Gate>();
    auto& gate = *sharedGate;
    auto connection = bus.subscribe<Dss::Core::ProcessingSessionResetEvent>(
        [sharedGate](const auto&) { sharedGate->enter(); });
    ReleaseGate release{gate};
    const auto path = frame("a.raw");
    ASSERT_TRUE(session->selectFiles({path}));
    ASSERT_TRUE(gate.waitEntered());
    auto closing = std::async(std::launch::async, [&] { session->shutdown(); });
    EXPECT_EQ(closing.wait_for(20ms), std::future_status::timeout);
    gate.release();
    ASSERT_EQ(closing.wait_for(3s), std::future_status::ready);
    closing.get();
    EXPECT_EQ(session->snapshot().state, State::Closed);
    EXPECT_FALSE(session->start());
    EXPECT_FALSE(session->step());
    EXPECT_FALSE(session->seek(0));
    EXPECT_FALSE(session->selectFiles({path}));
    session->stop();
    session->shutdown();
    EXPECT_EQ(started.load(), 0);
    EXPECT_EQ(session->snapshot().state, State::Closed);
}

TEST_F(ReplayApplicationSession, StopReturnsBeforeDrainAndRejectsReconfiguration) {
    auto sharedGate = std::make_shared<Gate>();
    auto& gate = *sharedGate;
    ReleaseGate release{gate};
    source->setFrameInterval(2s);
    const auto path = frame("a.raw");
    ASSERT_TRUE(select({path, path}));
    processor->setProcessingStrategy(std::make_unique<GatedProcessing>(sharedGate));
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(gate.waitEntered());
    session->stop();
    EXPECT_EQ(session->snapshot().state, State::Stopping);
    EXPECT_FALSE(session->seek(0));
    EXPECT_FALSE(session->step());
    EXPECT_FALSE(session->selectFiles({path}));
    EXPECT_FALSE(session->start());
    gate.release();
    ASSERT_TRUE(settled());
    EXPECT_EQ(session->snapshot().state, State::Idle);
    EXPECT_EQ(processed.load(), 1);
    EXPECT_FALSE(processor->isRunning());
    processor->setProcessingStrategy(nullptr);
}

TEST_F(ReplayApplicationSession, LastFrameFailureIsFailedAndCanRecover) {
    class Failure final : public Dss::Processing::IProcessingStrategy {
    public:
        auto process(const Dss::Processing::FramePacket&)
            -> Dss::Processing::ProcessingResult override {
            throw std::runtime_error("last frame failure");
        }
        auto mode() const -> Dss::Core::ProcessingMode override {
            return Dss::Core::ProcessingMode::Direct;
        }
        auto name() const -> std::string_view override {
            return "failure";
        }
    };
    ASSERT_TRUE(select({frame("a.raw")}));
    processor->setProcessingStrategy(std::make_unique<Failure>());
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(waitUntil([&] { return session->snapshot().state == State::Failed; }));
    EXPECT_NE(session->snapshot().error.find("processing failed"), std::string::npos);
    session->stop();
    EXPECT_EQ(session->snapshot().state, State::Failed);
    processor->setProcessingStrategy(nullptr);
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(waitUntil([&] { return session->snapshot().state == State::Completed; }));
    EXPECT_EQ(processed.load(), 1);
    EXPECT_EQ(resets.load(), 2);
    EXPECT_EQ(started.load(), stopped.load());
}

TEST_F(ReplayApplicationSession, DecodeFailureCanReloadRepairedSequence) {
    const auto missing = directory / "missing.raw";
    ASSERT_TRUE(select({frame("a.raw"), missing}));
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(waitUntil([&] { return session->snapshot().state == State::Failed; }));
    EXPECT_NE(session->snapshot().error.find("missing.raw"), std::string::npos);
    (void)frame("missing.raw");
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(waitUntil([&] { return session->snapshot().state == State::Completed; }));
    EXPECT_EQ(processed.load(), 3);
    EXPECT_EQ(resets.load(), 2);
}

TEST_F(ReplayApplicationSession, SeekWhileRunningResumesAtRequestedFrame) {
    source->setFrameInterval(2s);
    const auto path = frame("a.raw");
    ASSERT_TRUE(select(std::vector<std::filesystem::path>(5, path)));
    ASSERT_TRUE(session->start());
    ASSERT_TRUE(waitUntil([&] { return processed.load() == 1; }));
    ASSERT_TRUE(session->seek(3));
    ASSERT_TRUE(waitUntil(
        [&] { return processed.load() == 2 && session->snapshot().state == State::Running; }));
    session->stop();
    ASSERT_TRUE(settled());
    EXPECT_EQ(source->nextFrameIndex(), 4U);
    EXPECT_EQ(resets.load(), 2);
    EXPECT_EQ(started.load(), 2);
    EXPECT_EQ(stopped.load(), 2);
}

TEST_F(ReplayApplicationSession, StopCancelsLosslessSingleStepSubmission) {
    ASSERT_TRUE(select({frame("a.raw")}));
    auto entered = std::make_shared<std::promise<void>>();
    auto waiting = entered->get_future();
    source->setFrameCallback([entered](auto, auto context) {
        std::mutex mutex;
        std::condition_variable_any cv;
        std::unique_lock lock(mutex);
        entered->set_value();
        cv.wait(lock, context.stopToken, [] { return false; });
        return false;
    });
    ASSERT_TRUE(session->step());
    ASSERT_EQ(waiting.wait_for(3s), std::future_status::ready);
    session->stop();
    ASSERT_TRUE(settled());
    EXPECT_EQ(session->snapshot().state, State::Idle);
    EXPECT_EQ(source->nextFrameIndex(), 0U);
    EXPECT_FALSE(processor->isRunning());
    EXPECT_EQ(started.load(), 0);
}
