#include "dss/ui/view_model/replay_view_model.h"

#include <QTimer>
#include <exception>
#include <filesystem>
#include <limits>
#include <utility>

#include "dss/acquisition/source/frame_source_coordinator.h"
#include "dss/acquisition/source/i_frame_source.h"
#include "dss/acquisition/source/image_sequence_frame_source.h"
#include "dss/app/runtime_diagnostics.h"
#include "dss/app/service_keys.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/ui/support/qt_thread_utils.h"

namespace Dss::Ui {
namespace {

[[nodiscard]] auto boundedFrameCount(std::size_t count) -> int {
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(count);
}

[[nodiscard]] auto prepareReplaySource(
    const std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource>& replaySource,
    const std::shared_ptr<Dss::Acquisition::FrameSourceCoordinator>& coordinator)
    -> std::expected<void, std::string> {
    auto initialized = replaySource->init();
    if (!initialized.has_value()) {
        return initialized;
    }
    if (!coordinator) {
        return {};
    }
    return coordinator->selectSource(Dss::Acquisition::FrameSourceMode::Replay);
}

}  // namespace

ReplayViewModel::ReplayViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_bus(context.bus), m_registry(context.registry) {
    setupSubscriptions();
    auto* diagnosticsTimer = new QTimer(this);
    diagnosticsTimer->setInterval(1000);
    connect(diagnosticsTimer, &QTimer::timeout, this, &ReplayViewModel::refreshRuntimeDiagnostics);
    diagnosticsTimer->start();
    refreshRuntimeDiagnostics();
}

ReplayViewModel::~ReplayViewModel() {
    if (m_replayTaskWorker.joinable()) {
        m_replayTaskWorker.request_stop();
        m_replayTaskWorker.join();
    }
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

bool ReplayViewModel::selectReplayFiles(const QStringList& files) {
    if (m_replayBusy) {
        Q_EMIT statusTextChanged("Replay is loading");
        return false;
    }
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
        Dss::App::ServiceKey::replaySource);
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
    if (paths.empty()) {
        Q_EMIT statusTextChanged("Image sequence is empty");
        return false;
    }

    auto setFilesResult = replaySource->setFiles(std::move(paths));
    if (!setFilesResult.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(setFilesResult.error()));
        return false;
    }

    const auto frameCount = boundedFrameCount(replaySource->frameCount());
    setReplayFrameCount(frameCount);
    setReplayCurrentFrame(0);
    Q_EMIT statusTextChanged(QString("Sequence selected: %1 frames").arg(frameCount));
    return true;
}

void ReplayViewModel::startGrab() {
    if (m_replayBusy) {
        Q_EMIT statusTextChanged("Replay is loading");
        return;
    }
    if (m_grabbing) {
        return;
    }

    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
        Dss::App::ServiceKey::replaySource);
    auto frameSource =
        m_registry.tryGet<Dss::Acquisition::IFrameSource>(Dss::App::ServiceKey::frameSource);
    if (!frameSource) {
        frameSource = replaySource;
    }
    if (!processor || !replaySource || !frameSource) {
        Q_EMIT statusTextChanged("Replay services are not registered");
        return;
    }

    auto coordinator = m_registry.tryGet<Dss::Acquisition::FrameSourceCoordinator>(
        Dss::App::ServiceKey::frameSource);
    (void)startReplayTask("Replay: Loading", [replaySource, coordinator](std::stop_token token) {
        ReplayTaskResult result{};
        if (token.stop_requested()) {
            result.statusText = "Replay task canceled";
            return result;
        }

        auto prepared = prepareReplaySource(replaySource, coordinator);
        if (!prepared.has_value()) {
            result.statusText = QString::fromStdString(prepared.error());
            return result;
        }

        result.success = true;
        result.startReplayAfterCompletion = true;
        return result;
    });
}

void ReplayViewModel::startInitializedReplay() {
    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    auto frameSource =
        m_registry.tryGet<Dss::Acquisition::IFrameSource>(Dss::App::ServiceKey::frameSource);
    if (!frameSource) {
        frameSource =
            m_registry.tryGet<Dss::Acquisition::IFrameSource>(Dss::App::ServiceKey::replaySource);
    }
    if (!processor || !frameSource || frameSource->frameWidth() == 0U ||
        frameSource->frameHeight() == 0U) {
        Q_EMIT statusTextChanged("Replay services are not ready");
        return;
    }

    if (auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
            Dss::App::ServiceKey::replaySource);
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
    auto frameSource =
        m_registry.tryGet<Dss::Acquisition::IFrameSource>(Dss::App::ServiceKey::frameSource);
    if (!frameSource) {
        frameSource =
            m_registry.tryGet<Dss::Acquisition::IFrameSource>(Dss::App::ServiceKey::replaySource);
    }
    if (frameSource) {
        frameSource->stop();
    }

    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
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

    auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
        Dss::App::ServiceKey::replaySource);
    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    if (!replaySource || !processor) {
        Q_EMIT statusTextChanged("Replay services are not registered");
        return false;
    }

    auto coordinator = m_registry.tryGet<Dss::Acquisition::FrameSourceCoordinator>(
        Dss::App::ServiceKey::frameSource);

    return startReplayTask("Replay: Loading",
                           [replaySource, coordinator, processor](std::stop_token token) {
                               ReplayTaskResult result{};
                               if (token.stop_requested()) {
                                   result.statusText = "Replay task canceled";
                                   return result;
                               }

                               auto prepared = prepareReplaySource(replaySource, coordinator);
                               if (!prepared.has_value()) {
                                   result.statusText = QString::fromStdString(prepared.error());
                                   return result;
                               }

                               processor->start();
                               auto stepResult = replaySource->stepForward(token);
                               if (!stepResult.has_value()) {
                                   result.statusText = QString::fromStdString(stepResult.error());
                                   return result;
                               }

                               result.success = true;
                               result.statusText = "Replay: Ready";
                               return result;
                           });
}

bool ReplayViewModel::stepReplayBackward() {
    auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
        Dss::App::ServiceKey::replaySource);
    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    if (!replaySource || !processor) {
        Q_EMIT statusTextChanged("Replay services are not registered");
        return false;
    }
    if (m_grabbing) {
        stopGrab();
    }

    auto coordinator = m_registry.tryGet<Dss::Acquisition::FrameSourceCoordinator>(
        Dss::App::ServiceKey::frameSource);

    return startReplayTask("Replay: Loading",
                           [replaySource, coordinator, processor](std::stop_token token) {
                               ReplayTaskResult result{};
                               if (token.stop_requested()) {
                                   result.statusText = "Replay task canceled";
                                   return result;
                               }

                               auto prepared = prepareReplaySource(replaySource, coordinator);
                               if (!prepared.has_value()) {
                                   result.statusText = QString::fromStdString(prepared.error());
                                   return result;
                               }

                               const auto next = replaySource->nextFrameIndex();
                               const auto target = next >= 2U ? next - 2U : 0U;
                               auto seekResult = replaySource->seek(target);
                               if (!seekResult.has_value()) {
                                   result.statusText = QString::fromStdString(seekResult.error());
                                   return result;
                               }

                               if (token.stop_requested()) {
                                   result.statusText = "Replay task canceled";
                                   return result;
                               }

                               processor->start();
                               auto stepResult = replaySource->stepForward(token);
                               if (!stepResult.has_value()) {
                                   result.statusText = QString::fromStdString(stepResult.error());
                                   return result;
                               }

                               result.success = true;
                               result.statusText = "Replay: Ready";
                               return result;
                           });
}

bool ReplayViewModel::seekReplayFrame(int index) {
    if (m_replayBusy) {
        Q_EMIT statusTextChanged("Replay is loading");
        return false;
    }

    auto replaySource = m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>(
        Dss::App::ServiceKey::replaySource);
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

bool ReplayViewModel::startReplayTask(const QString& loadingText, ReplayTask task) {
    if (m_replayBusy) {
        Q_EMIT statusTextChanged("Replay is loading");
        return false;
    }
    if (!task) {
        Q_EMIT statusTextChanged("Replay task is invalid");
        return false;
    }

    if (m_replayTaskWorker.joinable()) {
        m_replayTaskWorker.request_stop();
        m_replayTaskWorker.join();
    }

    setReplayBusy(true);
    Q_EMIT statusTextChanged(loadingText);
    m_replayTaskWorker =
        std::jthread([this, task = std::move(task)](std::stop_token token) mutable {
            ReplayTaskResult result{};
            try {
                result = task(token);
            } catch (const std::exception& ex) {
                result.statusText = QString("Replay task failed: %1").arg(ex.what());
            } catch (...) {
                result.statusText = "Replay task failed";
            }

            invokeOnObjectThread(this, [this, result] { finishReplayTask(result); });
        });
    return true;
}

void ReplayViewModel::finishReplayTask(const ReplayTaskResult& result) {
    if (result.frameCount.has_value()) {
        setReplayFrameCount(*result.frameCount);
    }
    if (result.currentFrame.has_value()) {
        setReplayCurrentFrame(*result.currentFrame);
    }
    setReplayBusy(false);
    if (result.success && result.startReplayAfterCompletion && !m_stopRequestedAfterLoad) {
        startInitializedReplay();
    }
    m_stopRequestedAfterLoad = false;
    if (!result.statusText.isEmpty()) {
        Q_EMIT statusTextChanged(result.statusText);
    }
}

void ReplayViewModel::requestStopAfterLoad() {
    m_stopRequestedAfterLoad = true;
}

void ReplayViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));
}

void ReplayViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    const auto frame = static_cast<int>(event.frameSeq) + 1;
    invokeOnObjectThread(this, [this, frame] { setReplayCurrentFrame(frame); });
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
