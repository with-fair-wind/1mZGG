#pragma once

#include <QObject>
#include <QString>
#include <cstddef>
#include <expected>

#include "dss/core/config.h"

namespace Dss::Ui {

struct SettingsSnapshot {
    QString dataRoot;
    bool logEnabled = true;
    QString logFilePath;
    std::size_t logMaxFileSizeBytes = 0;
    std::size_t logMaxFiles = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    double pixelScale = 0.0;
};

class SettingsViewModel : public QObject {
    Q_OBJECT

public:
    explicit SettingsViewModel(Dss::Core::Config& config = Dss::Core::Config::instance(),
                               QObject* parent = nullptr);

    [[nodiscard]] auto snapshot() const -> SettingsSnapshot;
    auto save(const SettingsSnapshot& settings) -> std::expected<void, QString>;

Q_SIGNALS:
    void settingsSaved();
    void statusTextChanged(const QString& text);

private:
    Dss::Core::Config& m_config;
};

}  // namespace Dss::Ui
