#include "dss/ui/view_model/replay_view_model.h"

#include <QTimer>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <utility>

#include "dss/app/replay_session.h"
#include "dss/app/runtime_diagnostics.h"
#include "dss/app/service_keys.h"
#include "dss/ui/support/qt_thread_utils.h"

namespace Dss::Ui {
namespace {
int boundedFrameCount(std::size_t count) {
    return static_cast<int>(
        std::min(count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}
}  // namespace

ReplayViewModel::ReplayViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent),
      m_bus(context.bus),
      m_registry(context.registry),
      m_session(m_registry.tryGet<Dss::App::ReplaySession>(Dss::App::ServiceKey::replaySession)) {
    setupSubscriptions();
    auto* diagnosticsTimer = new QTimer(this);
    diagnosticsTimer->setInterval(1000);
    connect(diagnosticsTimer, &QTimer::timeout, this, &ReplayViewModel::refreshRuntimeDiagnostics);
    diagnosticsTimer->start();
    auto* sessionTimer = new QTimer(this);
    sessionTimer->setInterval(25);
    connect(sessionTimer, &QTimer::timeout, this, &ReplayViewModel::refreshSession);
    sessionTimer->start();
    refreshRuntimeDiagnostics();
    refreshSession();
}

ReplayViewModel::~ReplayViewModel() {
    shutdown();
}

void ReplayViewModel::shutdown() {
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    if (m_session) {
        m_session->shutdown();
    }
    m_grabbing = false;
    m_replayBusy = false;
}

bool ReplayViewModel::isGrabbing() const {
    return m_grabbing;
}

bool ReplayViewModel::replayBusy() const {
    return m_replayBusy;
}

int ReplayViewModel::replayFrameCount() const {
    return m_replayFrameCount;
}

int ReplayViewModel::replayCurrentFrame() const {
    return m_replayCurrentFrame;
}

auto ReplayViewModel::runtimeDiagnosticsText() const -> QString {
    return m_runtimeDiagnosticsText;
}

bool ReplayViewModel::canSubmit() {
    if (m_shutdown) {
        return false;
    }
    if (!m_session) {
        Q_EMIT statusTextChanged("Replay session is not registered");
        return false;
    }
    if (m_replayBusy) {
        Q_EMIT statusTextChanged("Replay is busy");
        return false;
    }
    return true;
}

bool ReplayViewModel::acceptCommand(const std::expected<void, std::string>& result) {
    if (!result) {
        Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        return false;
    }
    setReplayBusy(true);
    Q_EMIT statusTextChanged("Replay: Loading");
    return true;
}

bool ReplayViewModel::selectReplayFiles(const QStringList& files) {
    if (!canSubmit()) {
        return false;
    }
    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const auto& file : files) {
        if (!file.isEmpty()) {
            paths.emplace_back(file.toStdWString());
        }
    }
    return acceptCommand(m_session->selectFiles(std::move(paths)));
}

void ReplayViewModel::startGrab() {
    if (canSubmit()) {
        (void)acceptCommand(m_session->start());
    }
}

void ReplayViewModel::stopGrab() {
    if (!m_shutdown && m_session) {
        m_session->stop();
        refreshSession();
    }
}

void ReplayViewModel::requestStopAfterLoad() {
    stopGrab();
}

bool ReplayViewModel::stepReplayForward() {
    return canSubmit() && acceptCommand(m_session->step());
}

bool ReplayViewModel::stepReplayBackward() {
    return canSubmit() && acceptCommand(m_session->step(true));
}

bool ReplayViewModel::seekReplayFrame(int index) {
    if (!canSubmit()) {
        return false;
    }
    if (index < 0) {
        Q_EMIT statusTextChanged("Replay frame index is invalid");
        return false;
    }
    return acceptCommand(m_session->seek(static_cast<std::size_t>(index)));
}

void ReplayViewModel::refreshSession() {
    if (m_shutdown || !m_session) {
        return;
    }
    const auto value = m_session->snapshot();
    if (value.revision == m_snapshotRevision) {
        return;
    }
    m_snapshotRevision = value.revision;
    setReplayFrameCount(boundedFrameCount(value.frameCount));
    setGrabbing(value.state == Dss::App::ReplaySession::State::Running);
    // 状态和文本先就绪，最后发 busy=false，允许信号消费者安全提交下一条命令。
    using Notice = Dss::App::ReplaySession::Notice;
    QString text;
    switch (value.notice) {
        case Notice::None:
            break;
        case Notice::Loading:
            text = "Replay: Loading";
            break;
        case Notice::Selected:
            text = QString("Sequence selected: %1 frames").arg(m_replayFrameCount);
            break;
        case Notice::Playing:
            text = "Replaying...";
            break;
        case Notice::Stopping:
            text = "Replay: Stopping";
            break;
        case Notice::Stopped:
            text = "Stopped";
            break;
        case Notice::Ready:
            text = "Replay: Ready";
            break;
        case Notice::Positioned:
            text = QString("Replay positioned at frame %1").arg(value.position + 1);
            break;
        case Notice::Finished:
            text = "Replay finished";
            break;
        case Notice::Error:
            text = QString("Replay failed: %1").arg(QString::fromStdString(value.error));
            break;
    }
    if (!text.isEmpty()) {
        Q_EMIT statusTextChanged(text);
    }
    setReplayBusy(value.busy());
}

void ReplayViewModel::refreshRuntimeDiagnostics() {
    auto diagnostics =
        m_registry.tryGet<Dss::App::RuntimeDiagnostics>(Dss::App::ServiceKey::runtimeDiagnostics);
    if (!diagnostics) {
        return;
    }

    const auto value = diagnostics->snapshot();
    const auto text = QString("Drop P:%1 I:%2 T:%3 | Write I:%4/%5 T:%6/%7 | Err N:%8 S:%9 D:%10")
                          .arg(value.processingDroppedFrames)
                          .arg(value.imageDroppedRequests)
                          .arg(value.trackDroppedRequests)
                          .arg(value.imageSuccessfulWrites)
                          .arg(value.imageFailedWrites)
                          .arg(value.trackSuccessfulWrites)
                          .arg(value.trackFailedWrites)
                          .arg(value.networkErrors)
                          .arg(value.serialErrors)
                          .arg(value.storageErrors);
    if (text == m_runtimeDiagnosticsText) {
        return;
    }
    m_runtimeDiagnosticsText = text;
    Q_EMIT runtimeDiagnosticsTextChanged(text);
}

void ReplayViewModel::setupSubscriptions() {
    m_connections.push_back(
        m_bus.subscribe<Dss::Core::ProcessingSessionResetEvent>([this](const auto&) {
            const auto revision = ++m_sessionRevision;
            invokeOnObjectThread(this, [this, revision] {
                if (!m_shutdown && revision == m_sessionRevision.load()) {
                    setReplayCurrentFrame(0);
                }
            });
        }));
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));
}

void ReplayViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    const auto frame = boundedFrameCount(
        std::min(event.frameSeq, static_cast<std::uint64_t>(std::numeric_limits<int>::max() - 1)) +
        1);
    const auto revision = m_sessionRevision.load();
    invokeOnObjectThread(this, [this, frame, revision] {
        if (!m_shutdown && revision == m_sessionRevision.load()) {
            setReplayCurrentFrame(frame);
        }
    });
}

void ReplayViewModel::setReplayCurrentFrame(int frame) {
    if (m_replayCurrentFrame == frame) {
        return;
    }
    m_replayCurrentFrame = frame;
    Q_EMIT replayCurrentFrameChanged(frame);
}

void ReplayViewModel::setReplayFrameCount(int count) {
    if (m_replayFrameCount == count) {
        return;
    }
    m_replayFrameCount = count;
    Q_EMIT replayFrameCountChanged(count);
}

void ReplayViewModel::setGrabbing(bool value) {
    if (m_grabbing == value) {
        return;
    }
    m_grabbing = value;
    Q_EMIT grabbingChanged(value);
}

void ReplayViewModel::setReplayBusy(bool value) {
    if (m_replayBusy == value) {
        return;
    }
    m_replayBusy = value;
    Q_EMIT replayBusyChanged(value);
}

}  // namespace Dss::Ui
