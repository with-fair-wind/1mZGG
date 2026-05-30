#include "dss/core/config.h"

#include <QSettings>
#include <QString>

namespace Dss::Core
{

auto Config::load(const std::filesystem::path& iniPath) -> std::expected<void, std::string>
{
    if (!std::filesystem::exists(iniPath))
    {
        return std::unexpected(std::string("Config file not found: ") + iniPath.string());
    }

    QSettings settings(QString::fromStdString(iniPath.string()), QSettings::IniFormat);

    m_paths.iniFile = iniPath;

    // [Path]
    m_paths.dataRoot = settings.value("Path/DataRoot", "").toString().toStdString();
    m_paths.ccfFile = settings.value("Path/CCFFile", "").toString().toStdString();
    m_paths.kernelFile = settings.value("Path/KernelFile", "").toString().toStdString();

    // [CommNetSettings]
    m_commNet.displayPort.portName =
        settings.value("CommNetSettings/DisplayPort", "/dev/ttyUSB0S").toString().toStdString();
    m_commNet.exposurePort.portName =
        settings.value("CommNetSettings/ExposurePort", "/dev/ttyUSB1S").toString().toStdString();
    m_commNet.masterControlPort.portName =
        settings.value("CommNetSettings/MasterControlPort", "/dev/ttyUSB2S").toString().toStdString();
    m_commNet.servoPort.portName =
        settings.value("CommNetSettings/ServoPort", "/dev/ttyUSB3S").toString().toStdString();

    m_commNet.imageSender.remoteIp =
        settings.value("CommNetSettings/IPImageTrans", "192.168.1.2").toString().toStdString();
    m_commNet.imageSender.remotePort =
        static_cast<uint16_t>(settings.value("CommNetSettings/PortImageTrans", 4000).toInt());

    m_commNet.exchange.remoteIp =
        settings.value("CommNetSettings/IPExchange", "192.168.1.3").toString().toStdString();

    // [OpticSettings]
    m_optics.imageWidth = settings.value("OpticSettings/ImageWidth", 6144).toInt();
    m_optics.imageHeight = settings.value("OpticSettings/ImageHeight", 6144).toInt();
    m_optics.pixelScale = settings.value("OpticSettings/PixelScale", 0.0003453).toFloat();
    m_optics.fovCenterX = static_cast<float>(m_optics.imageWidth) / 2.0f;
    m_optics.fovCenterY = static_cast<float>(m_optics.imageHeight) / 2.0f;

    // [ObservatorySettings]
    m_observatory.longitude = settings.value("ObservatorySettings/Longitude", 0.0).toDouble();
    m_observatory.latitude = settings.value("ObservatorySettings/Latitude", 0.0).toDouble();
    m_observatory.altitude = settings.value("ObservatorySettings/Altitude", 0.0).toDouble();

    // [TrackSettings]
    m_tracking.thresholdLiving = settings.value("TrackSettings/ThresholdLiving", 0.5).toFloat();
    m_tracking.numFramesLiving = settings.value("TrackSettings/NumFramesLiving", 10).toInt();
    m_tracking.searchRadius = settings.value("TrackSettings/SearchRadius", 50.0).toFloat();
    m_tracking.ratioFov = settings.value("TrackSettings/RatioFOV", 0.25).toFloat();
    m_tracking.opticParams = m_optics;

    return {};
}

auto Config::save() -> std::expected<void, std::string>
{
    if (m_paths.iniFile.empty())
    {
        return std::unexpected("No config file path set");
    }

    QSettings settings(QString::fromStdString(m_paths.iniFile.string()), QSettings::IniFormat);

    settings.setValue("OpticSettings/ImageWidth", m_optics.imageWidth);
    settings.setValue("OpticSettings/ImageHeight", m_optics.imageHeight);
    settings.setValue("OpticSettings/PixelScale", static_cast<double>(m_optics.pixelScale));

    settings.setValue("TrackSettings/ThresholdLiving", static_cast<double>(m_tracking.thresholdLiving));
    settings.setValue("TrackSettings/NumFramesLiving", m_tracking.numFramesLiving);
    settings.setValue("TrackSettings/SearchRadius", static_cast<double>(m_tracking.searchRadius));

    settings.sync();
    return {};
}

} // namespace Dss::Core
