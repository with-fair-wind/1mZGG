#include "dss/ui/view_model.h"

namespace Dss::Ui
{

ViewModel::ViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry, QObject* parent)
    : QObject(parent)
    , m_bus(bus)
    , m_registry(registry)
{
    setupSubscriptions();
}

ViewModel::~ViewModel() = default;

void ViewModel::startGrab()
{
    // TODO: get IFrameSource from registry and start it
    m_grabbing = true;
    m_statusText = "Grabbing...";
    Q_EMIT grabbingChanged(true);
    Q_EMIT statusTextChanged(m_statusText);
}

void ViewModel::stopGrab()
{
    m_grabbing = false;
    m_statusText = "Stopped";
    Q_EMIT grabbingChanged(false);
    Q_EMIT statusTextChanged(m_statusText);
}

void ViewModel::setTrackMode(int mode)
{
    if (m_trackMode != mode)
    {
        m_trackMode = mode;
        Q_EMIT trackModeChanged(mode);
    }
}

void ViewModel::setExposure(double ms)
{
    if (m_exposure != ms)
    {
        m_exposure = ms;
        Q_EMIT exposureChanged(ms);
    }
}

void ViewModel::selectTarget(QPointF pos)
{
    m_bus.emit(Dss::Core::ManualTargetSelectEvent{
        static_cast<float>(pos.x()),
        static_cast<float>(pos.y())});
}

void ViewModel::startSaving()
{
    m_saving = true;
    Q_EMIT savingChanged(true);
}

void ViewModel::stopSaving()
{
    m_saving = false;
    Q_EMIT savingChanged(false);
}

void ViewModel::toggleZoom(int level)
{
    m_bus.emit(Dss::Core::ZoomChangeEvent{level});
}

void ViewModel::setupSubscriptions()
{
    m_connections.push_back(
        m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
            [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));

    m_connections.push_back(
        m_bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
            [this](const Dss::Core::ProcessingCompleteEvent& e) { onProcessingComplete(e); }));

    m_connections.push_back(
        m_bus.subscribe<Dss::Core::TrackResultEvent>(
            [this](const Dss::Core::TrackResultEvent& e) { onTrackResult(e); }));

    m_connections.push_back(
        m_bus.subscribe<Dss::Core::MasterControlEvent>(
            [this](const Dss::Core::MasterControlEvent& e) { onMasterControl(e); }));
}

void ViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event)
{
    if (!event.displayImage || event.width == 0 || event.height == 0 || event.stride == 0)
    {
        return;
    }

    const auto expectedSize = static_cast<size_t>(event.stride) * static_cast<size_t>(event.height);
    if (event.displayImage->size() < expectedSize)
    {
        return;
    }

    QImage image(
        event.displayImage->data(),
        static_cast<int>(event.width),
        static_cast<int>(event.height),
        static_cast<qsizetype>(event.stride),
        QImage::Format_Grayscale8);
    Q_EMIT displayImageReady(image.copy());
}

void ViewModel::onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event)
{
    const auto& s = event.stats;
    Q_EMIT imageStatsUpdated(s.minVal, s.maxVal, s.avg, s.stdDev);
}

void ViewModel::onTrackResult(const Dss::Core::TrackResultEvent& event)
{
    Q_EMIT targetListUpdated(static_cast<int>(event.targets.size()));

    if (!event.targets.empty())
    {
        const auto& t = event.targets.front();
        auto info = QString("Target: %1 | AZ: %2 EL: %3")
                        .arg(QString::fromStdString(t.targetId))
                        .arg(static_cast<double>(t.predictedPosAe.x), 0, 'f', 4)
                        .arg(static_cast<double>(t.predictedPosAe.y), 0, 'f', 4);
        Q_EMIT trackInfoUpdated(info);
    }
}

void ViewModel::onMasterControl(const Dss::Core::MasterControlEvent& event)
{
    setExposure(event.exposure);
    setTrackMode(event.trackMode);

    if (event.save && !m_saving)
    {
        startSaving();
    }
    else if (!event.save && m_saving)
    {
        stopSaving();
    }

    if (event.grab && !m_grabbing)
    {
        startGrab();
    }
    else if (!event.grab && m_grabbing)
    {
        stopGrab();
    }
}

} // namespace Dss::Ui
