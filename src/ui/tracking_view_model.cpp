#include "dss/ui/tracking_view_model.h"

#include <utility>

#include "dss/core/config.h"
#include "dss/processing/image_processor.h"
#include "dss/tracking/manual_tracker.h"
#include "dss/tracking/track_manager.h"

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
    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [this](const Dss::Core::TrackResultEvent& e) { onTrackResult(e); }));
}

void TrackingViewModel::onTrackResult(const Dss::Core::TrackResultEvent& event) {
    Q_EMIT targetListUpdated(static_cast<int>(event.targets.size()));

    if (!event.targets.empty()) {
        const auto& t = event.targets.front();
        auto info = QString("Target: %1 | AZ: %2 EL: %3")
                        .arg(QString::fromStdString(t.targetId))
                        .arg(static_cast<double>(t.predictedPosAe.x), 0, 'f', 4)
                        .arg(static_cast<double>(t.predictedPosAe.y), 0, 'f', 4);
        Q_EMIT trackInfoUpdated(info);
    }
}

void TrackingViewModel::configureTrackingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        Q_EMIT statusTextChanged("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::TrackMode>(m_trackMode);
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
