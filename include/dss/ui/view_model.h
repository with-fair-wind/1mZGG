#pragma once

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <memory>
#include <optional>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/core/types.h"

namespace Dss::Ui {

class ViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isGrabbing READ isGrabbing NOTIFY grabbingChanged)
    Q_PROPERTY(double frameRate READ frameRate NOTIFY frameRateChanged)
    Q_PROPERTY(
        int processingMode READ processingMode WRITE setProcessingMode NOTIFY processingModeChanged)
    Q_PROPERTY(int trackMode READ trackMode WRITE setTrackMode NOTIFY trackModeChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY exposureChanged)
    Q_PROPERTY(bool isSaving READ isSaving NOTIFY savingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(int replayFrameCount READ replayFrameCount NOTIFY replayFrameCountChanged)
    Q_PROPERTY(int replayCurrentFrame READ replayCurrentFrame NOTIFY replayCurrentFrameChanged)

public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit ViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry,
                       QObject* parent = nullptr);
    ~ViewModel() override;

    [[nodiscard]] bool isGrabbing() const {
        return m_grabbing;
    }
    [[nodiscard]] double frameRate() const {
        return m_frameRate;
    }
    [[nodiscard]] int processingMode() const {
        return m_processingMode;
    }
    [[nodiscard]] int trackMode() const {
        return m_trackMode;
    }
    [[nodiscard]] double exposure() const {
        return m_exposure;
    }
    [[nodiscard]] bool isSaving() const {
        return m_saving;
    }
    [[nodiscard]] QString statusText() const {
        return m_statusText;
    }
    [[nodiscard]] int replayFrameCount() const {
        return m_replayFrameCount;
    }
    [[nodiscard]] int replayCurrentFrame() const {
        return m_replayCurrentFrame;
    }

    Q_INVOKABLE bool selectReplayFiles(const QStringList& files);
    Q_INVOKABLE void startGrab();
    Q_INVOKABLE void stopGrab();
    Q_INVOKABLE bool stepReplayForward();
    Q_INVOKABLE void setProcessingMode(int mode);
    Q_INVOKABLE void setTrackMode(int mode);
    Q_INVOKABLE void setExposure(double ms);
    Q_INVOKABLE void selectTarget(QPointF pos);
    Q_INVOKABLE void startSaving();
    Q_INVOKABLE void stopSaving();
    Q_INVOKABLE void toggleZoom(int level);

signals:
    void grabbingChanged(bool value);
    void frameRateChanged(double value);
    void processingModeChanged(int mode);
    void trackModeChanged(int mode);
    void exposureChanged(double ms);
    void savingChanged(bool value);
    void statusTextChanged(const QString& text);
    void replayFrameCountChanged(int count);
    void replayCurrentFrameChanged(int frame);

    void displayImageReady(QImage image);
    void cropImageReady(QImage image);
    void trackInfoUpdated(const QString& info);
    void targetListUpdated(int count);
    void imageStatsUpdated(double minVal, double maxVal, double avg, double stdDev);

private:
    void setupSubscriptions();

    void onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event);
    void onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event);
    void onTrackResult(const Dss::Core::TrackResultEvent& event);
    void onMasterControl(const Dss::Core::MasterControlEvent& event);
    void configureProcessingStrategy();
    void configureTrackingStrategy();
    [[nodiscard]] static auto makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob;
    void setReplayCurrentFrame(int frame);
    void setStatus(QString text);

    MessageBus& m_bus;
    Dss::Core::ServiceRegistry& m_registry;

    bool m_grabbing = false;
    double m_frameRate = 0.0;
    int m_processingMode = static_cast<int>(Dss::Core::ProcessingMode::None);
    int m_trackMode = static_cast<int>(Dss::Core::TrackMode::Init);
    double m_exposure = 0.0;
    bool m_saving = false;
    QString m_statusText = "Ready";
    int m_replayFrameCount = 0;
    int m_replayCurrentFrame = 0;
    std::optional<Dss::Core::MeasuredBlob> m_manualTarget;

    std::vector<Dss::Evt::ScopedConnection> m_connections;
};

}  // namespace Dss::Ui
