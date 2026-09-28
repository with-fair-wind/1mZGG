#include "dss/ui/view_model/storage_view_model.h"

#include <QDateTime>

#include "dss/app/service_keys.h"
#include "dss/core/config/config.h"
#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#include "dss/ui/support/qt_thread_utils.h"

namespace Dss::Ui {

StorageViewModel::StorageViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

StorageViewModel::~StorageViewModel() {
    shutdown();
}

void StorageViewModel::shutdown() {
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    m_pendingSession.reset();
    stopSaving();
    if (m_stopWorker.joinable()) {
        m_stopWorker.join();
    }
}

bool StorageViewModel::isSaving() const {
    return m_saving;
}

void StorageViewModel::startSaving() {
    const auto sessionTimestamp =
        QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmss").toStdString();
    startSaving(Dss::Storage::ImageStorageNaming{
        .startTime = sessionTimestamp,
        .endTime = sessionTimestamp,
        .observatoryId = Dss::Core::Config::instance().observatory().id,
        .imageFormat = "raw",
        .searchMode = true,
    });
}

void StorageViewModel::startSaving(const Dss::Storage::ImageStorageNaming& naming) {
    if (m_shutdown) {
        return;
    }
    if (m_saving) {
        stopSaving();
    }
    if (m_stopping) {
        m_pendingSession = naming;
        return;
    }
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>(
        Dss::App::ServiceKey::imageStorage);
    if (!storage) {
        Q_EMIT statusTextChanged("Image storage is not registered");
        return;
    }
    if (!storage->isReady()) {
        auto initResult = storage->init(storage->baseDir());
        if (!initResult.has_value()) {
            Q_EMIT statusTextChanged(QString::fromStdString(initResult.error()));
            return;
        }
    }

    auto trackStorage = m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>(
        Dss::App::ServiceKey::trackDataStorage);
    if (trackStorage && !trackStorage->isReady()) {
        auto initTrackResult = trackStorage->init(trackStorage->baseDir());
        if (!initTrackResult.has_value()) {
            Q_EMIT statusTextChanged(QString::fromStdString(initTrackResult.error()));
            return;
        }
    }

    auto imageSessionResult = storage->configureSession(naming);
    if (!imageSessionResult.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(imageSessionResult.error()));
        return;
    }
    if (trackStorage) {
        auto trackSessionResult = trackStorage->configureSession(naming);
        if (!trackSessionResult.has_value()) {
            Q_EMIT statusTextChanged(QString::fromStdString(trackSessionResult.error()));
            return;
        }
    }

    auto startResult = storage->start();
    if (!startResult.has_value()) {
        Q_EMIT statusTextChanged(QString::fromStdString(startResult.error()));
        return;
    }
    if (trackStorage) {
        auto startTrackResult = trackStorage->start();
        if (!startTrackResult.has_value()) {
            storage->stop();
            Q_EMIT statusTextChanged(QString::fromStdString(startTrackResult.error()));
            return;
        }
    }

    setSaving(true);
    Q_EMIT statusTextChanged("Saving enabled");
}

void StorageViewModel::stopSaving() {
    m_pendingSession.reset();
    if (m_stopping) {
        return;
    }
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>(
        Dss::App::ServiceKey::imageStorage);
    if (storage) {
        storage->requestStop();
    }
    auto trackStorage = m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>(
        Dss::App::ServiceKey::trackDataStorage);
    if (trackStorage) {
        trackStorage->requestStop();
    }
    m_stopping = true;
    setSaving(false);
    Q_EMIT stoppingChanged(true);
    Q_EMIT statusTextChanged("Saving: draining");
    m_stopWorker = std::jthread([this, storage, trackStorage] {
        if (storage) {
            storage->stop();
        }
        if (trackStorage) {
            trackStorage->stop();
        }
        invokeOnObjectThread(this, [this] { finishStop(); });
    });
}

void StorageViewModel::finishStop() {
    if (m_shutdown) {
        return;
    }
    if (m_stopWorker.joinable()) {
        m_stopWorker.join();
    }
    m_stopping = false;
    Q_EMIT stoppingChanged(false);
    Q_EMIT statusTextChanged("Saving stopped");
    auto pending = std::exchange(m_pendingSession, std::nullopt);
    if (pending) {
        startSaving(*pending);
    }
}

void StorageViewModel::setSaving(bool value) {
    if (m_saving == value) {
        return;
    }
    m_saving = value;
    Q_EMIT savingChanged(value);
}

}  // namespace Dss::Ui
