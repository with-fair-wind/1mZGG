#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "dss/core/constants.h"
#include "dss/core/config_types.h"
#include "dss/core/types.h"

namespace Dss::Core
{

struct PathConfig
{
    std::filesystem::path iniFile;
    std::filesystem::path dataRoot;
    std::filesystem::path ccfFile;
    std::filesystem::path kernelFile;
};

struct CommNetConfig
{
    SerialConfig displayPort;
    SerialConfig exposurePort;
    SerialConfig masterControlPort;
    SerialConfig servoPort;
    std::string cameraPort;

    UdpEndpointConfig imageSender;
    UdpEndpointConfig exchange;
    UdpEndpointConfig errorDiag;
    UdpEndpointConfig atmos;
    UdpEndpointConfig heartbeat;
};

struct ObservatoryConfig
{
    double longitude = 0.0;
    double latitude = 0.0;
    double altitude = 0.0;
};

class Config
{
public:
    static auto instance() -> Config&
    {
        static Config config;
        return config;
    }

    auto load(const std::filesystem::path& iniPath) -> std::expected<void, std::string>;
    auto save() -> std::expected<void, std::string>;

    [[nodiscard]] auto paths() const -> const PathConfig& { return m_paths; }
    [[nodiscard]] auto commNet() const -> const CommNetConfig& { return m_commNet; }
    [[nodiscard]] auto optics() const -> const OpticParams& { return m_optics; }
    [[nodiscard]] auto observatory() const -> const ObservatoryConfig& { return m_observatory; }
    [[nodiscard]] auto trackingSettings() const -> const TrackingSettings& { return m_tracking; }

    auto mutablePaths() -> PathConfig& { return m_paths; }
    auto mutableCommNet() -> CommNetConfig& { return m_commNet; }
    auto mutableOptics() -> OpticParams& { return m_optics; }
    auto mutableTracking() -> TrackingSettings& { return m_tracking; }

private:
    Config() = default;

    PathConfig m_paths{};
    CommNetConfig m_commNet{};
    OpticParams m_optics{};
    ObservatoryConfig m_observatory{};
    TrackingSettings m_tracking{};
};

} // namespace Dss::Core
