#include "dss/ui/replay_view_model.h"

#include <QTimer>
#include <filesystem>
#include <utility>

#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/app/runtime_diagnostics.h"
#include "dss/processing/image_processor.h"

namespace Dss::Ui {

ReplayViewModel::ReplayViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_bus(context.bus), m_registry(context.registry) {
    setupSubscriptions();
    auto* diagnosticsTimer = new QTimer(this);
    diagnosticsTimer->setInterval(1000);
    connect(diagnosticsTimer, &QTimer::timeout, this, &ReplayViewModel::refreshRuntimeDiagnostics);
    diagnosticsTimer->start();
    refreshRuntimeDiagnostics();
}

ReplayViewModel::~ReplayViewModel() = default;

bool ReplayViewModel::isGrabbing() const {
    return m_grabbing;
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

bool ReplayViewModel::selectReplayFiles(const QStringList& files) {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        Q_EMIT statusTextChanged("Replay source is not registered");
        return false;
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const auto& file : files) {
        if (!file.isEmpty()) {
            paths.emplace_back(file.toStdWString());
        }
    }

    auto result = replaySource->setFiles(std::move(paths));
    if (!result.has_value()) {
        setReplayFrameCount(0);
        setReplayCurrentFrame(0);
        Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        return false;
    }

    auto initResult = replaySource->init();
    if (!initResult.has_value()) {
        setReplayFrameCount(0);
        setReplayCurrentFrame(0);
        Q_EMIT statusTextChanged(QString::fromStdString(initResult.error()));
        return false;
    }

    setReplayFrameCount(static_cast<int>(replaySource->frameCount()));
    setReplayCurrentFrame(0);
    Q_EMIT statusTextChanged(QString("Sequence selected: %1 frames").arg(m_replayFrameCount));
    return true;
}

void ReplayViewModel::startGrab() {
    if (m_grabbing) {
        return;
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (!processor || !frameSource) {
        Q_EMIT statusTextChanged("Replay services are not registered");
        return;
    }

    if (frameSource->frameWidth() == 0U || frameSource->frameHeight() == 0U) {
        auto initResult = frameSource->init();
        if (!initResult.has_value()) {
            Q_EMIT statusTextChanged(QString::fromStdString(initResult.error()));
            return;
        }
        setReplayCurrentFrame(0);
    }

    if (auto replaySource =
            m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
        replaySource && replaySource->nextFrameIndex() >= replaySource->frameCount()) {
        (void)replaySource->seek(0);
        setReplayCurrentFrame(0);
    }
    processor->start();
    frameSource->start();
    setGrabbing(true);
    Q_EMIT statusTextChanged("Replaying...");
    m_bus.emit(Dss::Core::GrabStartedEvent{frameSource->frameWidth(), frameSource->frameHeight()});
}

void ReplayViewModel::stopGrab() {
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (frameSource) {
        frameSource->stop();
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (processor) {
        processor->stop();
    }

    setGrabbing(false);
    Q_EMIT statusTextChanged("Stopped");
    m_bus.emit(Dss::Core::GrabStoppedEvent{});
}

bool ReplayViewModel::stepReplayForward() {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        Q_EMIT statusTextChanged("Replay source is not registered");
        return false;
    }

    auto result = replaySource->stepForward();
    if (!result.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        return false;
    }
    return true;
}

bool ReplayViewModel::stepReplayBackward() {
    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        Q_EMIT statusTextChanged("Replay source is not registered");
        return false;
    }
    if (m_grabbing) {
        stopGrab();
    }

    const auto next = replaySource->nextFrameIndex();
    const auto target = next >= 2U ? next - 2U : 0U;
    auto seekResult = replaySource->seek(target);
    if (!seekResult.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(seekResult.error()));
        return false;
    }
    return stepReplayForward();
}

bool ReplayViewModel::seekReplayFrame(int index) {
    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource || index < 0) {
        Q_EMIT statusTextChanged("Replay frame index is invalid");
        return false;
    }

    auto result = replaySource->seek(static_cast<std::size_t>(index));
    if (!result.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        return false;
    }
    Q_EMIT statusTextChanged(QString("Replay positioned at frame %1").arg(index + 1));
    return true;
}

void ReplayViewModel::refreshRuntimeDiagnostics() {
    auto diagnostics = m_registry.tryGet<Dss::App::RuntimeDiagnostics>("runtime_diagnostics");
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
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));
}

void ReplayViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    setReplayCurrentFrame(static_cast<int>(event.frameSeq) + 1);
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

}  // namespace Dss::Ui
