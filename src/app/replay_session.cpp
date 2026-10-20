#include "dss/app/replay_session.h"

#include <chrono>
#include <stdexcept>
#include <utility>

#include "dss/acquisition/source/frame_source_coordinator.h"
#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/core/event/events.h"
#include "dss/processing/pipeline/image_processor.h"

namespace Dss::App {
namespace {
/// 将依赖返回的错误送至服务线程唯一异常边界。
void requireSuccess(const std::expected<void, std::string>& result) {
    if (!result) {
        throw std::runtime_error(result.error());
    }
}
}  // namespace

ReplaySession::ReplaySession(Dss::Core::MessageBus& bus,
                             std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource> source,
                             std::shared_ptr<Dss::Processing::ImageProcessor> processor,
                             std::shared_ptr<Dss::Acquisition::FrameSourceCoordinator> coordinator,
                             CompletionBarrier completionBarrier)
    : m_bus(bus),
      m_source(std::move(source)),
      m_processor(std::move(processor)),
      m_coordinator(std::move(coordinator)),
      m_completionBarrier(std::move(completionBarrier)) {
    if (!m_source || !m_processor) {
        throw std::invalid_argument("Replay services are not registered");
    }
    m_worker = std::jthread([this] { run(); });
}

ReplaySession::~ReplaySession() {
    shutdown();
}

auto ReplaySession::snapshot() const -> Snapshot {
    std::scoped_lock lock(m_mutex);
    return m_snapshot;
}

auto ReplaySession::selectFiles(std::vector<std::filesystem::path> files)
    -> std::expected<void, std::string> {
    if (files.empty()) {
        return std::unexpected("Image sequence is empty");
    }
    return submit({.operation = Operation::Select, .files = std::move(files)});
}

auto ReplaySession::start() -> std::expected<void, std::string> {
    return submit({.operation = Operation::Start, .files = {}});
}

auto ReplaySession::step(bool backward) -> std::expected<void, std::string> {
    return submit({.operation = backward ? Operation::Backward : Operation::Forward, .files = {}});
}

auto ReplaySession::seek(std::size_t index) -> std::expected<void, std::string> {
    return submit({.operation = Operation::Seek, .files = {}, .index = index});
}

auto ReplaySession::submit(Command command) -> std::expected<void, std::string> {
    std::scoped_lock lock(m_mutex);
    if (m_closed) {
        return std::unexpected("Replay session is closed");
    }
    if (m_snapshot.busy()) {
        return std::unexpected("Replay is busy");
    }
    if (command.operation == Operation::Start && m_snapshot.state == State::Running) {
        return std::unexpected("Replay is already running");
    }
    if (command.operation == Operation::Seek && command.index >= m_snapshot.frameCount) {
        return std::unexpected("Replay frame index is invalid");
    }
    command.resume = m_snapshot.state == State::Running;
    command.recover = m_snapshot.state == State::Failed;
    m_cancel = std::stop_source{};
    m_pending = std::move(command);
    m_snapshot.state = State::Loading;
    m_snapshot.notice = Notice::Loading;
    m_snapshot.error.clear();
    ++m_snapshot.revision;
    m_wake.notify_one();
    return {};
}

void ReplaySession::stop() {
    std::stop_source cancel;
    {
        std::scoped_lock lock(m_mutex);
        if (m_closed || m_stopRequested) {
            return;
        }
        // 终态上的停止不能抹掉 Failed，后续 start 仍须重新加载。
        if (m_snapshot.state != State::Running && !m_snapshot.busy()) {
            return;
        }
        m_stopRequested = true;
        m_snapshot.state = State::Stopping;
        m_snapshot.notice = Notice::Stopping;
        ++m_snapshot.revision;
        cancel = m_cancel;
    }
    cancel.request_stop();  // stop_callback 可能执行用户代码，不能持有状态锁。
    m_processor->cancelPendingResults();
    m_wake.notify_one();
}

void ReplaySession::shutdown() {
    std::scoped_lock shutdownLock(m_shutdownMutex);
    std::stop_source cancel;
    {
        std::scoped_lock lock(m_mutex);
        m_closed = true;
        cancel = m_cancel;
    }
    cancel.request_stop();
    m_processor->cancelPendingResults();
    m_wake.notify_one();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void ReplaySession::publish(State state, Notice notice, std::string error, std::size_t position) {
    std::scoped_lock lock(m_mutex);
    if (m_closed || m_stopRequested) {
        return;
    }
    m_snapshot.state = state;
    m_snapshot.notice = notice;
    m_snapshot.error = std::move(error);
    m_snapshot.frameCount = m_files.size();
    m_snapshot.position = position;
    ++m_snapshot.revision;
}

void ReplaySession::stopSources() {
    if (m_coordinator) {
        m_coordinator->stop();
    }
    m_source->stop();
}

void ReplaySession::notifyStopped() {
    if (std::exchange(m_announcedRunning, false)) {
        m_bus.emit(Dss::Core::GrabStoppedEvent{});
    }
}

void ReplaySession::beginPlayback(std::stop_token token) {
    if (token.stop_requested()) {
        return;
    }
    m_processor->start();
    if (token.stop_requested()) {
        return;
    }
    m_source->start();
    m_announcedRunning = true;
    m_bus.emit(Dss::Core::GrabStartedEvent{m_source->frameWidth(), m_source->frameHeight()});
    publish(State::Running, Notice::Playing);
}

auto ReplaySession::finishAcceptedWrites(std::stop_token token) -> bool {
    if (token.stop_requested()) {
        return false;
    }
    if (m_completionBarrier) {
        requireSuccess(m_completionBarrier(token));
    }
    return !token.stop_requested();
}

void ReplaySession::execute(Command command, std::stop_token token) {
    m_processor->cancelPendingResults();
    stopSources();
    m_processor->drain();
    notifyStopped();
    if (token.stop_requested()) {
        return;
    }
    if (command.operation == Operation::Select) {
        requireSuccess(m_source->setFiles(command.files));
        m_files = std::move(command.files);
        m_processor->resetSession();
        publish(State::Idle, Notice::Selected);
        return;
    }
    const auto completion = m_source->completion();
    if (command.recover || m_processor->hasFailed() || (completion && !completion->error.empty())) {
        requireSuccess(m_source->setFiles(m_files));
        m_processor->resetSession();
    }
    if (token.stop_requested()) {
        return;
    }
    requireSuccess(m_source->init());
    if (token.stop_requested()) {
        return;
    }
    if (m_coordinator) {
        requireSuccess(m_coordinator->selectSource(Dss::Acquisition::FrameSourceMode::Replay));
    }
    if (command.operation == Operation::Seek || command.operation == Operation::Backward) {
        const auto next = m_source->nextFrameIndex();
        auto index = next > 1 ? next - 2 : 0;
        if (command.operation == Operation::Seek) {
            index = command.index;
        }
        m_processor->resetSession();
        requireSuccess(m_source->seek(index));
    }
    if (command.operation == Operation::Seek) {
        if (command.resume) {
            beginPlayback(token);
        } else {
            publish(State::Idle, Notice::Positioned, {}, command.index);
        }
        return;
    }
    if (command.operation == Operation::Start) {
        if (m_source->nextFrameIndex() >= m_source->frameCount()) {
            m_processor->resetSession();
            requireSuccess(m_source->seek(0));
        }
        beginPlayback(token);
        return;
    }
    if (token.stop_requested()) {
        return;
    }
    if (m_source->nextFrameIndex() >= m_source->frameCount()) {
        publish(State::Completed, Notice::Finished);
        return;
    }
    m_processor->start();
    requireSuccess(m_source->stepForward(token));
    m_processor->drain();
    if (m_processor->hasFailed()) {
        throw std::runtime_error("processing failed; see diagnostics");
    }
    if (finishAcceptedWrites(token)) {
        publish(State::Idle, Notice::Ready);
    }
}

void ReplaySession::finishStopRequest() {
    stopSources();
    m_processor->drain();
    notifyStopped();
    const auto completion = m_source->completion();
    std::string error;
    if (m_processor->hasFailed()) {
        error = "processing failed; see diagnostics";
    } else if (completion) {
        error = completion->error;
    }
    // 回收结束前保持 Stopping，不能提前接收新命令。
    std::scoped_lock lock(m_mutex);
    m_stopRequested = false;
    m_snapshot.state = error.empty() ? State::Idle : State::Failed;
    m_snapshot.notice = error.empty() ? Notice::Stopped : Notice::Error;
    m_snapshot.error = error;
    m_snapshot.frameCount = m_files.size();
    ++m_snapshot.revision;
}

void ReplaySession::pollCompletion() {
    const auto completion = m_source->completion();
    if (completion || m_processor->hasFailed()) {
        std::stop_token token;
        {
            std::scoped_lock lock(m_mutex);
            // 查询 EOF 期间可能收到普通命令；已接受的命令优先，不能覆盖其 Loading。
            if (m_closed || m_pending || m_stopRequested || m_snapshot.state != State::Running) {
                return;
            }
            token = m_cancel.get_token();
            m_snapshot.state = State::Stopping;
            m_snapshot.notice = Notice::Stopping;
            ++m_snapshot.revision;
        }
        stopSources();
        m_processor->drain();
        notifyStopped();
        if (!m_processor->hasFailed() && !finishAcceptedWrites(token)) {
            return;
        }
        std::string error;
        if (m_processor->hasFailed()) {
            error = "processing failed; see diagnostics";
        } else if (completion) {
            error = completion->error;
        }
        publish(error.empty() ? State::Completed : State::Failed,
                error.empty() ? Notice::Finished : Notice::Error, error);
    }
}

void ReplaySession::run() {
    for (;;) {
        std::optional<Command> command;
        std::stop_token token;
        bool stopRequested = false;
        {
            std::unique_lock lock(m_mutex);
            const auto ready = [this] { return m_closed || m_stopRequested || m_pending; };
            if (m_snapshot.state == State::Running) {
                m_wake.wait_for(lock, std::chrono::milliseconds{25}, ready);
            } else {
                m_wake.wait(lock, ready);
            }
            if (m_closed) {
                break;
            }
            stopRequested = m_stopRequested;
            command = std::exchange(m_pending, std::nullopt);
            token = m_cancel.get_token();
        }
        try {
            if (stopRequested) {
                finishStopRequest();
            } else if (command) {
                execute(std::move(*command), token);
            } else {
                pollCompletion();
            }
        } catch (const std::exception& error) {
            stopSources();
            m_processor->stop();
            notifyStopped();
            publish(State::Failed, Notice::Error, error.what());
        } catch (...) {
            stopSources();
            m_processor->stop();
            notifyStopped();
            publish(State::Failed, Notice::Error, "unknown replay failure");
        }
    }
    stopSources();
    m_processor->stop();
    notifyStopped();
    std::scoped_lock lock(m_mutex);
    m_pending.reset();
    m_snapshot.state = State::Closed;
    m_snapshot.notice = Notice::None;
    ++m_snapshot.revision;
}

}  // namespace Dss::App
