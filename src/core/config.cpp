#include "dss/core/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace Dss::Core
{

namespace
{

using Section = std::unordered_map<std::string, std::string>;
using IniData = std::unordered_map<std::string, Section>;

[[nodiscard]] auto trim(std::string_view value) -> std::string_view
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    auto last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}

[[nodiscard]] auto normalizeBool(std::string value) -> std::string
{
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] auto parseIni(const std::filesystem::path& iniPath) -> std::expected<IniData, std::string>
{
    std::ifstream input(iniPath);
    if (!input)
    {
        return std::unexpected("Failed to open config file: " + iniPath.string());
    }

    IniData data;
    std::string currentSection;
    std::string line;
    while (std::getline(input, line))
    {
        const auto commentPos = line.find_first_of("#;");
        if (commentPos != std::string::npos)
        {
            line.erase(commentPos);
        }

        const auto view = trim(line);
        if (view.empty())
        {
            continue;
        }

        if (view.front() == '[' && view.back() == ']')
        {
            currentSection = std::string(trim(view.substr(1, view.size() - 2)));
            data.try_emplace(currentSection);
            continue;
        }

        const auto equalPos = view.find('=');
        if (equalPos == std::string_view::npos)
        {
            continue;
        }

        const auto key = std::string(trim(view.substr(0, equalPos)));
        const auto value = std::string(trim(view.substr(equalPos + 1)));
        data[currentSection][key] = value;
    }

    return data;
}

[[nodiscard]] auto getString(const IniData& data,
                             std::string_view section,
                             std::string_view key,
                             std::string fallback) -> std::string
{
    const auto sectionIt = data.find(std::string(section));
    if (sectionIt == data.end())
    {
        return fallback;
    }

    const auto valueIt = sectionIt->second.find(std::string(key));
    return valueIt != sectionIt->second.end() ? valueIt->second : std::move(fallback);
}

[[nodiscard]] auto getInt(const IniData& data,
                          std::string_view section,
                          std::string_view key,
                          int fallback) -> int
{
    try
    {
        return std::stoi(getString(data, section, key, std::to_string(fallback)));
    }
    catch (...)
    {
        return fallback;
    }
}

[[nodiscard]] auto getUInt16(const IniData& data,
                             std::string_view section,
                             std::string_view key,
                             uint16_t fallback) -> uint16_t
{
    const auto value = getInt(data, section, key, fallback);
    if (value < 0 || value > static_cast<int>(std::numeric_limits<uint16_t>::max()))
    {
        return fallback;
    }
    return static_cast<uint16_t>(value);
}

[[nodiscard]] auto getDouble(const IniData& data,
                             std::string_view section,
                             std::string_view key,
                             double fallback) -> double
{
    try
    {
        return std::stod(getString(data, section, key, std::to_string(fallback)));
    }
    catch (...)
    {
        return fallback;
    }
}

[[nodiscard]] auto getFloat(const IniData& data,
                            std::string_view section,
                            std::string_view key,
                            float fallback) -> float
{
    return static_cast<float>(getDouble(data, section, key, fallback));
}

[[nodiscard]] auto getBool(const IniData& data,
                           std::string_view section,
                           std::string_view key,
                           bool fallback) -> bool
{
    const auto value = normalizeBool(getString(data, section, key, fallback ? "true" : "false"));
    if (value == "true" || value == "1" || value == "yes" || value == "on")
    {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off")
    {
        return false;
    }
    return fallback;
}

void writeEndpoint(std::ostream& output,
                   std::string_view ipKey,
                   std::string_view portKey,
                   const UdpEndpointConfig& endpoint)
{
    output << ipKey << '=' << endpoint.remoteIp << '\n';
    output << portKey << '=' << endpoint.remotePort << '\n';
}

} // namespace

auto Config::load(const std::filesystem::path& iniPath) -> std::expected<void, std::string>
{
    if (!std::filesystem::exists(iniPath))
    {
        return std::unexpected("Config file not found: " + iniPath.string());
    }

    auto parsed = parseIni(iniPath);
    if (!parsed)
    {
        return std::unexpected(parsed.error());
    }

    const auto& settings = *parsed;
    m_paths.iniFile = iniPath;

    m_paths.dataRoot = getString(settings, "Path", "DataRoot", "");
    m_paths.ccfFile = getString(settings, "Path", "CCFFile", "");
    m_paths.kernelFile = getString(settings, "Path", "KernelFile", "");

    m_commNet.displayPort.portName = getString(settings, "CommNetSettings", "DisplayPort", "/dev/ttyUSB0S");
    m_commNet.exposurePort.portName = getString(settings, "CommNetSettings", "ExposurePort", "/dev/ttyUSB1S");
    m_commNet.masterControlPort.portName =
        getString(settings, "CommNetSettings", "MasterControlPort", "/dev/ttyUSB2S");
    m_commNet.servoPort.portName = getString(settings, "CommNetSettings", "ServoPort", "/dev/ttyUSB3S");
    m_commNet.cameraPort = getString(settings, "CommNetSettings", "CameraPort", "/dev/ttyUSB4");

    m_commNet.imageSender.remoteIp =
        getString(settings, "CommNetSettings", "IPImageTrans", "192.168.1.2");
    m_commNet.imageSender.remotePort = getUInt16(settings, "CommNetSettings", "PortImageTrans", 4000);
    m_commNet.exchange.remoteIp = getString(settings, "CommNetSettings", "IPExchange", "192.168.1.3");
    m_commNet.exchange.remotePort = getUInt16(settings, "CommNetSettings", "PortExchange", 5000);
    m_commNet.errorDiag.remoteIp = getString(settings, "CommNetSettings", "IPErrorDiag", "192.168.1.4");
    m_commNet.errorDiag.remotePort = getUInt16(settings, "CommNetSettings", "PortErrorDiag", 5001);
    m_commNet.atmos.remoteIp = getString(settings, "CommNetSettings", "IPAtmos", "192.168.1.5");
    m_commNet.atmos.remotePort = getUInt16(settings, "CommNetSettings", "PortAtmos", 5002);
    m_commNet.heartbeat.remoteIp = getString(settings, "CommNetSettings", "IPHeartbeat", "192.168.1.6");
    m_commNet.heartbeat.remotePort = getUInt16(settings, "CommNetSettings", "PortHeartbeat", 5003);

    m_optics.imageWidth = getInt(settings, "OpticSettings", "ImageWidth", 6144);
    m_optics.imageHeight = getInt(settings, "OpticSettings", "ImageHeight", 6144);
    m_optics.pixelScale = getFloat(settings, "OpticSettings", "PixelScale", 0.0003453f);
    m_optics.fovCenterX = static_cast<float>(m_optics.imageWidth) / 2.0f;
    m_optics.fovCenterY = static_cast<float>(m_optics.imageHeight) / 2.0f;

    m_observatory.longitude = getDouble(settings, "ObservatorySettings", "Longitude", 0.0);
    m_observatory.latitude = getDouble(settings, "ObservatorySettings", "Latitude", 0.0);
    m_observatory.altitude = getDouble(settings, "ObservatorySettings", "Altitude", 0.0);

    m_tracking.thresholdLiving = getFloat(settings, "TrackSettings", "ThresholdLiving", 0.5f);
    m_tracking.numFramesLiving = getInt(settings, "TrackSettings", "NumFramesLiving", 10);
    m_tracking.searchRadius = getFloat(settings, "TrackSettings", "SearchRadius", 50.0f);
    m_tracking.ratioFov = getFloat(settings, "TrackSettings", "RatioFOV", 0.25f);
    m_tracking.thresholdStarMode = getFloat(settings, "TrackSettings", "ThresholdStarMode", 10.0f);
    m_tracking.thresholdGazeMode = getFloat(settings, "TrackSettings", "ThresholdGazeMode", 2.0f);
    m_tracking.autoDecide = getBool(settings, "TrackSettings", "AutoDecide", true);
    m_tracking.opticParams = m_optics;

    return {};
}

auto Config::save() -> std::expected<void, std::string>
{
    if (m_paths.iniFile.empty())
    {
        return std::unexpected("No config file path set");
    }

    std::ofstream output(m_paths.iniFile);
    if (!output)
    {
        return std::unexpected("Failed to open config file for writing: " + m_paths.iniFile.string());
    }

    output << "[Path]\n";
    output << "DataRoot=" << m_paths.dataRoot.string() << '\n';
    output << "CCFFile=" << m_paths.ccfFile.string() << '\n';
    output << "KernelFile=" << m_paths.kernelFile.string() << "\n\n";

    output << "[CommNetSettings]\n";
    output << "DisplayPort=" << m_commNet.displayPort.portName << '\n';
    output << "ExposurePort=" << m_commNet.exposurePort.portName << '\n';
    output << "MasterControlPort=" << m_commNet.masterControlPort.portName << '\n';
    output << "ServoPort=" << m_commNet.servoPort.portName << '\n';
    output << "CameraPort=" << m_commNet.cameraPort << "\n\n";
    writeEndpoint(output, "IPImageTrans", "PortImageTrans", m_commNet.imageSender);
    writeEndpoint(output, "IPExchange", "PortExchange", m_commNet.exchange);
    writeEndpoint(output, "IPErrorDiag", "PortErrorDiag", m_commNet.errorDiag);
    writeEndpoint(output, "IPAtmos", "PortAtmos", m_commNet.atmos);
    writeEndpoint(output, "IPHeartbeat", "PortHeartbeat", m_commNet.heartbeat);

    output << "\n[OpticSettings]\n";
    output << "ImageWidth=" << m_optics.imageWidth << '\n';
    output << "ImageHeight=" << m_optics.imageHeight << '\n';
    output << "PixelScale=" << m_optics.pixelScale << "\n\n";

    output << "[ObservatorySettings]\n";
    output << "Longitude=" << m_observatory.longitude << '\n';
    output << "Latitude=" << m_observatory.latitude << '\n';
    output << "Altitude=" << m_observatory.altitude << "\n\n";

    output << "[TrackSettings]\n";
    output << "ThresholdLiving=" << m_tracking.thresholdLiving << '\n';
    output << "NumFramesLiving=" << m_tracking.numFramesLiving << '\n';
    output << "SearchRadius=" << m_tracking.searchRadius << '\n';
    output << "RatioFOV=" << m_tracking.ratioFov << '\n';
    output << "ThresholdStarMode=" << m_tracking.thresholdStarMode << '\n';
    output << "ThresholdGazeMode=" << m_tracking.thresholdGazeMode << '\n';
    output << "AutoDecide=" << (m_tracking.autoDecide ? "true" : "false") << '\n';

    return {};
}

} // namespace Dss::Core
