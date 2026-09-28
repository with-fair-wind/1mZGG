#include "dss/ui/view_model/tracking_view_model.h"

#include <algorithm>
#include <utility>

#include "dss/app/service_keys.h"
#include "dss/core/config/config.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/tracking/strategy/manual_tracker.h"
#include "dss/tracking/track_manager.h"
#include "dss/ui/support/qt_thread_utils.h"

namespace Dss::Ui {

TrackingViewModel::TrackingViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_bus(context.bus), m_registry(context.registry) {
    setupSubscriptions();
}

TrackingViewModel::~TrackingViewModel() = default;

int TrackingViewModel::trackMode() const {
    return m_trackMode;
}

void TrackingViewModel::setTrackMode(int mode) {
    if (m_trackMode == mode && m_strategyConfigured) {
        return;
    }
    if (m_trackMode != mode) {
        m_trackMode = mode;
        Q_EMIT trackModeChanged(mode);
    }
    configureTrackingStrategy();
}

void TrackingViewModel::selectTarget(QPointF pos) {
    m_manualTarget = makeManualTarget(pos);
    m_bus.emit(Dss::Core::ManualTargetSelectEvent{static_cast<float>(pos.x()),
                                                  static_cast<float>(pos.y())});
    if (m_trackMode != static_cast<int>(Dss::Core::TrackMode::Manual)) {
        setTrackMode(static_cast<int>(Dss::Core::TrackMode::Manual));
    } else {
        configureTrackingStrategy();
    }
    Q_EMIT statusTextChanged(
        QString("Manual target selected: %1, %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
}

void TrackingViewModel::setupSubscriptions() {
    m_connections.push_back(
        m_bus.subscribe<Dss::Core::ProcessingSessionResetEvent>([this](const auto&) {
            ++m_sessionRevision;
            onTrackResult({});
        }));
    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [this](const Dss::Core::TrackResultEvent& e) { onTrackResult(e); }));
}

void TrackingViewModel::onTrackResult(const Dss::Core::TrackResultEvent& event) {
    const auto revision = m_sessionRevision.load();
    DisplayResult result{};
    result.count =
        static_cast<int>(std::ranges::count(event.targets, true, &Dss::Core::TargetInfo::living));
    const auto active = std::ranges::find(event.targets, true, &Dss::Core::TargetInfo::living);
    if (active != event.targets.end()) {
        const auto& t = *active;
        result.info = QString("Target: %1 | AZ: %2 EL: %3")
                          .arg(QString::fromStdString(t.targetId))
                          .arg(static_cast<double>(t.predictedPosAe.x), 0, 'f', 4)
                          .arg(static_cast<double>(t.predictedPosAe.y), 0, 'f', 4);
    }
    bool schedule = false;
    {
        std::lock_guard lock(m_pendingMutex);
        if (revision != m_sessionRevision.load()) {
            return;
        }
        m_pendingResult = std::move(result);
        schedule = !std::exchange(m_updateQueued, true);
    }
    if (schedule && isObjectThread(this)) {
        flushPendingResult();
    } else if (schedule) {
        invokeOnObjectThread(this, [this] { flushPendingResult(); });
    }
}

void TrackingViewModel::flushPendingResult() {
    std::optional<DisplayResult> result;
    {
        std::lock_guard lock(m_pendingMutex);
        result = std::exchange(m_pendingResult, std::nullopt);
        m_updateQueued = false;
    }
    if (result) {
        Q_EMIT targetListUpdated(result->count);
        Q_EMIT trackInfoUpdated(result->info);
    }
}

void TrackingViewModel::configureTrackingStrategy() {
    auto processor =
        m_registry.tryGet<Dss::Processing::ImageProcessor>(Dss::App::ServiceKey::imageProcessor);
    if (!processor) {
        Q_EMIT statusTextChanged("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::TrackMode>(m_trackMode);
    m_strategyConfigured = true;
    auto strategy =
        Dss::Tracking::makeTrackingStrategy(mode, Dss::Core::Config::instance().trackingSettings());
    if (!strategy) {
        processor->setTrackingStrategy(nullptr);
        Q_EMIT statusTextChanged("Tracking disabled");
        return;
    }

    if (auto* manualTracker = dynamic_cast<Dss::Tracking::ManualTracker*>(strategy.get())) {
        if (m_manualTarget.has_value()) {
            manualTracker->setManualTarget(*m_manualTarget);
        }
        processor->setTrackingStrategy(std::move(strategy));
        Q_EMIT statusTextChanged(m_manualTarget.has_value() ? "Manual tracking target armed"
                                                            : "Manual tracking enabled");
        return;
    }

    processor->setTrackingStrategy(std::move(strategy));
    Q_EMIT statusTextChanged("Tracking mode enabled");
}

auto TrackingViewModel::makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.id = "manual";
    blob.centroid = Dss::Core::Vec2f{static_cast<float>(pos.x()), static_cast<float>(pos.y())};
    blob.minX = blob.centroid.x - 10.0F;
    blob.maxX = blob.centroid.x + 10.0F;
    blob.minY = blob.centroid.y - 10.0F;
    blob.maxY = blob.centroid.y + 10.0F;
    blob.area = 100.0F;
    blob.dn = 10000.0F;
    return blob;
}

}  // namespace Dss::Ui
