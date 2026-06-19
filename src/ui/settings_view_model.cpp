#include "dss/ui/settings_view_model.h"

#include <filesystem>

namespace Dss::Ui {

SettingsViewModel::SettingsViewModel(Dss::Core::Config& config, QObject* parent)
    : QObject(parent), m_config(config) {}

auto SettingsViewModel::snapshot() const -> SettingsSnapshot {
    const auto& paths = m_config.paths();
    const auto& logging = m_config.logging();
    const auto& optics = m_config.optics();
    return {
        .dataRoot = QString::fromStdString(paths.dataRoot.generic_string()),
        .logEnabled = logging.enabled,
        .logFilePath = QString::fromStdString(logging.filePath.generic_string()),
        .logMaxFileSizeBytes = logging.maxFileSizeBytes,
        .logMaxFiles = logging.maxFiles,
        .imageWidth = optics.imageWidth,
        .imageHeight = optics.imageHeight,
        .pixelScale = optics.pixelScale,
    };
}

auto SettingsViewModel::save(const SettingsSnapshot& settings) -> std::expected<void, QString> {
    if (settings.dataRoot.trimmed().isEmpty()) {
        return std::unexpected(QString{"Data root cannot be empty"});
    }
    if (settings.logEnabled && settings.logFilePath.trimmed().isEmpty()) {
        return std::unexpected(QString{"Log file path cannot be empty"});
    }
    if (settings.logMaxFileSizeBytes == 0U || settings.logMaxFiles == 0U) {
        return std::unexpected(QString{"Log rotation values must be greater than zero"});
    }
    if (settings.imageWidth <= 0 || settings.imageHeight <= 0 || settings.pixelScale <= 0.0) {
        return std::unexpected(QString{"Optical dimensions and pixel scale must be positive"});
    }

    const auto oldPaths = m_config.paths();
    const auto oldLogging = m_config.logging();
    const auto oldOptics = m_config.optics();
    m_config.mutablePaths().dataRoot =
        std::filesystem::path(settings.dataRoot.trimmed().toStdString());
    auto& logging = m_config.mutableLogging();
    logging.enabled = settings.logEnabled;
    logging.filePath = std::filesystem::path(settings.logFilePath.trimmed().toStdString());
    logging.maxFileSizeBytes = settings.logMaxFileSizeBytes;
    logging.maxFiles = settings.logMaxFiles;
    auto& optics = m_config.mutableOptics();
    optics.imageWidth = settings.imageWidth;
    optics.imageHeight = settings.imageHeight;
    optics.pixelScale = static_cast<float>(settings.pixelScale);
    m_config.mutableTracking().opticParams = optics;

    const auto result = m_config.save();
    if (!result) {
        m_config.mutablePaths() = oldPaths;
        m_config.mutableLogging() = oldLogging;
        m_config.mutableOptics() = oldOptics;
        m_config.mutableTracking().opticParams = oldOptics;
        return std::unexpected(QString::fromStdString(result.error()));
    }

    Q_EMIT settingsSaved();
    Q_EMIT statusTextChanged("Settings saved");
    return {};
}

}  // namespace Dss::Ui
