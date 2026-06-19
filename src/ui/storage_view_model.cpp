#include "dss/ui/storage_view_model.h"

#include <QDateTime>

#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"

namespace Dss::Ui {

StorageViewModel::StorageViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

StorageViewModel::~StorageViewModel() = default;

bool StorageViewModel::isSaving() const {
    return m_saving;
}

void StorageViewModel::startSaving() {
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
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

    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage && !trackStorage->isReady()) {
        auto initTrackResult = trackStorage->init(trackStorage->baseDir());
        if (!initTrackResult.has_value()) {
            Q_EMIT statusTextChanged(QString::fromStdString(initTrackResult.error()));
            return;
        }
    }

    const auto sessionTimestamp =
        QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmss").toStdString();
    Dss::Storage::ImageStorageNaming naming{
        .startTime = sessionTimestamp,
        .endTime = sessionTimestamp,
        .observatoryId = "0",
        .imageFormat = "raw",
        .searchMode = true,
    };
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
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
    if (storage) {
        storage->stop();
    }
    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage) {
        trackStorage->stop();
    }
    setSaving(false);
    Q_EMIT statusTextChanged("Saving stopped");
}

void StorageViewModel::setSaving(bool value) {
    if (m_saving == value) {
        return;
    }
    m_saving = value;
    Q_EMIT savingChanged(value);
}

}  // namespace Dss::Ui
