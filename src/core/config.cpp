#include "dss/core/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <string_view>

#include <nlohmann/json.hpp>

namespace Dss::Core {

namespace {

using Json = nlohmann::json;

[[nodiscard]] auto normalizeBool(std::string value) -> std::string {
    std::ranges::transform(value, value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

[[nodiscard]] auto parseJson(const std::filesystem::path& configPath)
    -> std::expected<Json, std::string> {
    std::ifstream input(configPath);
    if (!input) {
        return std::unexpected("Failed to open config file: " + configPath.string());
    }

    try {
        return Json::parse(input);
    } catch (const Json::exception& error) {
        return std::unexpected("Failed to parse JSON config file " + configPath.string() + ": " +
                               error.what());
    }
}

[[nodiscard]] auto objectAt(const Json& object, std::string_view key) -> const Json* {
    if (!object.is_object()) {
        return nullptr;
    }

    const auto it = object.find(std::string(key));
    return it != object.end() && it->is_object() ? &(*it) : nullptr;
}

[[nodiscard]] auto objectAt(const Json* object, std::string_view key) -> const Json* {
    return object != nullptr ? objectAt(*object, key) : nullptr;
}

[[nodiscard]] auto valueAt(const Json* object, std::string_view key) -> const Json* {
    if (object == nullptr || !object->is_object()) {
        return nullptr;
    }

    const auto it = object->find(std::string(key));
    return it != object->end() && !it->is_null() ? &(*it) : nullptr;
}

[[nodiscard]] auto getString(const Json* object, std::string_view key, std::string fallback)
    -> std::string {
    const auto* value = valueAt(object, key);
    return value != nullptr && value->is_string() ? value->get<std::string>() : std::move(fallback);
}

[[nodiscard]] auto getBool(const Json* object, std::string_view key, bool fallback) -> bool {
    const auto* value = valueAt(object, key);
    if (value == nullptr) {
        return fallback;
    }

    if (value->is_boolean()) {
        return value->get<bool>();
    }
    if (value->is_number_integer()) {
        return value->get<int>() != 0;
    }
    if (value->is_string()) {
        const auto normalized = normalizeBool(value->get<std::string>());
        if (normalized == "true" || normalized == "1" || normalized == "yes" ||
            normalized == "on") {
            return true;
        }
        if (normalized == "false" || normalized == "0" || normalized == "no" ||
            normalized == "off") {
            return false;
        }
    }
    return fallback;
}

[[nodiscard]] auto getDouble(const Json* object, std::string_view key, double fallback) -> double {
    const auto* value = valueAt(object, key);
    if (value == nullptr) {
        return fallback;
    }

    try {
        if (value->is_number()) {
            return value->get<double>();
        }
        if (value->is_string()) {
            return std::stod(value->get<std::string>());
        }
    } catch (...) {
    }
    return fallback;
}

[[nodiscard]] auto getFloat(const Json* object, std::string_view key, float fallback) -> float {
    return static_cast<float>(getDouble(object, key, fallback));
}

[[nodiscard]] auto getInt(const Json* object, std::string_view key, int fallback) -> int {
    const auto* value = valueAt(object, key);
    if (value == nullptr) {
        return fallback;
    }

    try {
        long long parsed = 0;
        if (value->is_number_integer()) {
            parsed = value->get<long long>();
        } else if (value->is_string()) {
            parsed = std::stoll(value->get<std::string>());
        } else {
            return fallback;
        }

        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
            return fallback;
        }
        return static_cast<int>(parsed);
    } catch (...) {
        return fallback;
    }
}

[[nodiscard]] auto getUInt16(const Json* object, std::string_view key, uint16_t fallback)
    -> uint16_t {
    const auto value = getInt(object, key, fallback);
    if (value < 0 || value > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
        return fallback;
    }
    return static_cast<uint16_t>(value);
}

[[nodiscard]] auto getPath(const Json* object, std::string_view key,
                           const std::filesystem::path& fallback) -> std::filesystem::path {
    return getString(object, key, fallback.string());
}

[[nodiscard]] auto loadSerial(const Json* object, std::string_view key, SerialConfig fallback)
    -> SerialConfig {
    const auto* section = objectAt(object, key);
    fallback.portName = getString(section, "portName", fallback.portName);
    fallback.baudRate = getInt(section, "baudRate", fallback.baudRate);
    fallback.dataBits = getInt(section, "dataBits", fallback.dataBits);
    fallback.stopBits = getInt(section, "stopBits", fallback.stopBits);
    return fallback;
}

[[nodiscard]] auto loadEndpoint(const Json* object, std::string_view key,
                                UdpEndpointConfig fallback) -> UdpEndpointConfig {
    const auto* section = objectAt(object, key);
    fallback.localIp = getString(section, "localIp", fallback.localIp);
    fallback.localPort = getUInt16(section, "localPort", fallback.localPort);
    fallback.remoteIp = getString(section, "remoteIp", fallback.remoteIp);
    fallback.remotePort = getUInt16(section, "remotePort", fallback.remotePort);
    return fallback;
}

/// 计算下一个 UDP 端口，到达上限时保持原值
[[nodiscard]] auto nextUdpPort(uint16_t port) -> uint16_t {
    if (port == std::numeric_limits<uint16_t>::max()) {
        return port;
    }
    return static_cast<uint16_t>(port + 1U);
}

/// 根据旧版 GXTC 端点推导 GDCL 端点，兼容 GDCL=GXTC+1 的默认端口约定
[[nodiscard]] auto makeLegacyGdclEndpoint(UdpEndpointConfig gxtcConfig) -> UdpEndpointConfig {
    if (gxtcConfig.localPort != 0) {
        gxtcConfig.localPort = nextUdpPort(gxtcConfig.localPort);
    }
    gxtcConfig.remotePort = nextUdpPort(gxtcConfig.remotePort);
    return gxtcConfig;
}

[[nodiscard]] auto serialToJson(const SerialConfig& config) -> Json {
    return {
        {"portName", config.portName},
        {"baudRate", config.baudRate},
        {"dataBits", config.dataBits},
        {"stopBits", config.stopBits},
    };
}

[[nodiscard]] auto endpointToJson(const UdpEndpointConfig& config) -> Json {
    return {
        {"localIp", config.localIp},
        {"localPort", config.localPort},
        {"remoteIp", config.remoteIp},
        {"remotePort", config.remotePort},
    };
}

}  // namespace

auto Config::load(const std::filesystem::path& configPath) -> std::expected<void, std::string> {
    if (!std::filesystem::exists(configPath)) {
        return std::unexpected("Config file not found: " + configPath.string());
    }

    auto parsed = parseJson(configPath);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }

    const auto& settings = *parsed;
    const auto* paths = objectAt(settings, "paths");
    const auto* logging = objectAt(settings, "logging");
    const auto* commNet = objectAt(settings, "commNet");
    const auto* optics = objectAt(settings, "optics");
    const auto* processing = objectAt(settings, "processing");
    const auto* observatory = objectAt(settings, "observatory");
    const auto* tracking = objectAt(settings, "tracking");

    m_paths.configFile = configPath;
    m_paths.dataRoot = getPath(paths, "dataRoot", {});
    m_paths.ccfFile = getPath(paths, "ccfFile", {});
    m_paths.kernelFile = getPath(paths, "kernelFile", {});

    m_logging.enabled = getBool(logging, "enabled", true);
    m_logging.filePath = getPath(logging, "filePath", "./logs/dss.log");
    const auto maxFileSizeBytes = getInt(logging, "maxFileSizeBytes", 10 * 1024 * 1024);
    const auto maxFiles = getInt(logging, "maxFiles", 5);
    m_logging.maxFileSizeBytes =
        maxFileSizeBytes > 0 ? static_cast<std::size_t>(maxFileSizeBytes) : 10U * 1024U * 1024U;
    m_logging.maxFiles = maxFiles > 0 ? static_cast<std::size_t>(maxFiles) : 5U;

    m_commNet.displayPort = loadSerial(commNet, "displayPort", {"/dev/ttyUSB0S", 230400, 8, 1});
    m_commNet.exposurePort = loadSerial(commNet, "exposurePort", {"/dev/ttyUSB1S", 230400, 8, 1});
    m_commNet.masterControlPort =
        loadSerial(commNet, "masterControlPort", {"/dev/ttyUSB2S", 230400, 8, 1});
    m_commNet.servoPort = loadSerial(commNet, "servoPort", {"/dev/ttyUSB3S", 230400, 8, 1});
    m_commNet.cameraPort = getString(commNet, "cameraPort", "/dev/ttyUSB4");

    m_commNet.imageSender = loadEndpoint(commNet, "imageSender", {"", 0, "192.168.1.2", 4000});
    m_commNet.exchange = loadEndpoint(commNet, "exchange", {"", 0, "192.168.1.3", 5000});
    m_commNet.exchangeGxtc = loadEndpoint(commNet, "exchangeGxtc", m_commNet.exchange);
    m_commNet.exchangeGdcl =
        loadEndpoint(commNet, "exchangeGdcl", makeLegacyGdclEndpoint(m_commNet.exchangeGxtc));
    m_commNet.errorDiag = loadEndpoint(commNet, "errorDiag", {"", 0, "192.168.1.4", 5001});
    m_commNet.atmos = loadEndpoint(commNet, "atmos", {"", 0, "192.168.1.5", 5002});
    m_commNet.heartbeat = loadEndpoint(commNet, "heartbeat", {"", 15361, "0.0.0.0", 15362});

    m_optics.imageWidth = getInt(optics, "imageWidth", 6144);
    m_optics.imageHeight = getInt(optics, "imageHeight", 6144);
    m_optics.pixelScale = getFloat(optics, "pixelScale", 0.0003453f);
    m_optics.fovCenterX =
        getFloat(optics, "fovCenterX", static_cast<float>(m_optics.imageWidth) / 2.0f);
    m_optics.fovCenterY =
        getFloat(optics, "fovCenterY", static_cast<float>(m_optics.imageHeight) / 2.0f);

    const auto thresholdSigma = getDouble(processing, "thresholdSigma", 3.0);
    m_processing.thresholdSigma = thresholdSigma > 0.0 ? thresholdSigma : 3.0;
    const auto diffThreshold = getInt(processing, "diffThreshold", 20);
    m_processing.diffThreshold =
        diffThreshold >= 0 && diffThreshold <= std::numeric_limits<std::uint16_t>::max()
            ? static_cast<std::uint16_t>(diffThreshold)
            : std::uint16_t{20};
    const auto minArea = getInt(processing, "minArea", 3);
    const auto maxArea = getInt(processing, "maxArea", 100000);
    const auto validAreaRange = minArea > 0 && maxArea >= minArea;
    m_processing.minArea = validAreaRange ? minArea : 3;
    m_processing.maxArea = validAreaRange ? maxArea : 100000;
    const auto displayLow = getInt(processing, "displayLow", 0);
    const auto displayHigh = getInt(processing, "displayHigh", 16384);
    const auto validDisplayWindow = displayLow >= 0 && displayHigh > displayLow &&
                                    displayHigh <= std::numeric_limits<std::uint16_t>::max();
    m_processing.displayLow =
        validDisplayWindow ? static_cast<std::uint16_t>(displayLow) : std::uint16_t{0};
    m_processing.displayHigh =
        validDisplayWindow ? static_cast<std::uint16_t>(displayHigh) : std::uint16_t{16384};

    m_observatory.id = getString(observatory, "id", "0");
    m_observatory.longitude = getDouble(observatory, "longitude", 0.0);
    m_observatory.latitude = getDouble(observatory, "latitude", 0.0);
    m_observatory.altitude = getDouble(observatory, "altitude", 0.0);

    m_tracking.thresholdLiving = getFloat(tracking, "thresholdLiving", 0.5f);
    m_tracking.numFramesLiving = getInt(tracking, "numFramesLiving", 10);
    m_tracking.searchRadius = getFloat(tracking, "searchRadius", 50.0f);
    m_tracking.ratioFov = getFloat(tracking, "ratioFov", 0.25f);
    m_tracking.thresholdStarMode = getFloat(tracking, "thresholdStarMode", 10.0f);
    m_tracking.thresholdGazeMode = getFloat(tracking, "thresholdGazeMode", 2.0f);
    m_tracking.autoDecide = getBool(tracking, "autoDecide", true);
    m_tracking.thresholdMeo = getFloat(tracking, "thresholdMeo", 5.0f);
    m_tracking.spdLowAe = getFloat(tracking, "spdLowAe", 0.0f);
    m_tracking.spdHighAe = getFloat(tracking, "spdHighAe", 0.0f);
    m_tracking.thresholdAe = getFloat(tracking, "thresholdAe", 0.0f);
    m_tracking.geoFullLeo = getBool(tracking, "geoFullLeo", true);
    m_tracking.geoRaThresholdArcsec = getDouble(tracking, "geoRaThresholdArcsec", 5.4);
    m_tracking.geoDecThresholdArcsec = getDouble(tracking, "geoDecThresholdArcsec", 3.0);
    m_tracking.geoRaSpeedThresholdArcsec = getDouble(tracking, "geoRaSpeedThresholdArcsec", 10.0);
    m_tracking.geoDecSpeedThresholdArcsec = getDouble(tracking, "geoDecSpeedThresholdArcsec", 6.0);
    m_tracking.opticParams = m_optics;

    return {};
}

auto Config::save() -> std::expected<void, std::string> {
    if (m_paths.configFile.empty()) {
        return std::unexpected("No config file path set");
    }

    const Json outputJson = {
        {"paths",
         {
             {"dataRoot", m_paths.dataRoot.string()},
             {"ccfFile", m_paths.ccfFile.string()},
             {"kernelFile", m_paths.kernelFile.string()},
         }},
        {"logging",
         {
             {"enabled", m_logging.enabled},
             {"filePath", m_logging.filePath.string()},
             {"maxFileSizeBytes", m_logging.maxFileSizeBytes},
             {"maxFiles", m_logging.maxFiles},
         }},
        {"commNet",
         {
             {"displayPort", serialToJson(m_commNet.displayPort)},
             {"exposurePort", serialToJson(m_commNet.exposurePort)},
             {"masterControlPort", serialToJson(m_commNet.masterControlPort)},
             {"servoPort", serialToJson(m_commNet.servoPort)},
             {"cameraPort", m_commNet.cameraPort},
             {"imageSender", endpointToJson(m_commNet.imageSender)},
             {"exchange", endpointToJson(m_commNet.exchange)},
             {"exchangeGxtc", endpointToJson(m_commNet.exchangeGxtc)},
             {"exchangeGdcl", endpointToJson(m_commNet.exchangeGdcl)},
             {"errorDiag", endpointToJson(m_commNet.errorDiag)},
             {"atmos", endpointToJson(m_commNet.atmos)},
             {"heartbeat", endpointToJson(m_commNet.heartbeat)},
         }},
        {"optics",
         {
             {"imageWidth", m_optics.imageWidth},
             {"imageHeight", m_optics.imageHeight},
             {"fovCenterX", m_optics.fovCenterX},
             {"fovCenterY", m_optics.fovCenterY},
             {"pixelScale", m_optics.pixelScale},
         }},
        {"processing",
         {
             {"thresholdSigma", m_processing.thresholdSigma},
             {"diffThreshold", m_processing.diffThreshold},
             {"minArea", m_processing.minArea},
             {"maxArea", m_processing.maxArea},
             {"displayLow", m_processing.displayLow},
             {"displayHigh", m_processing.displayHigh},
         }},
        {"observatory",
         {
             {"id", m_observatory.id},
             {"longitude", m_observatory.longitude},
             {"latitude", m_observatory.latitude},
             {"altitude", m_observatory.altitude},
         }},
        {"tracking",
         {
             {"thresholdLiving", m_tracking.thresholdLiving},
             {"numFramesLiving", m_tracking.numFramesLiving},
             {"searchRadius", m_tracking.searchRadius},
             {"ratioFov", m_tracking.ratioFov},
             {"thresholdStarMode", m_tracking.thresholdStarMode},
             {"thresholdGazeMode", m_tracking.thresholdGazeMode},
             {"autoDecide", m_tracking.autoDecide},
             {"thresholdMeo", m_tracking.thresholdMeo},
             {"spdLowAe", m_tracking.spdLowAe},
             {"spdHighAe", m_tracking.spdHighAe},
             {"thresholdAe", m_tracking.thresholdAe},
             {"geoFullLeo", m_tracking.geoFullLeo},
             {"geoRaThresholdArcsec", m_tracking.geoRaThresholdArcsec},
             {"geoDecThresholdArcsec", m_tracking.geoDecThresholdArcsec},
             {"geoRaSpeedThresholdArcsec", m_tracking.geoRaSpeedThresholdArcsec},
             {"geoDecSpeedThresholdArcsec", m_tracking.geoDecSpeedThresholdArcsec},
         }},
    };

    std::ofstream output(m_paths.configFile);
    if (!output) {
        return std::unexpected("Failed to open config file for writing: " +
                               m_paths.configFile.string());
    }

    output << outputJson.dump(4) << '\n';
    return {};
}

}  // namespace Dss::Core
