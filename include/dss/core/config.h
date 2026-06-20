#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

#include "dss/core/config_types.h"
#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Core {

/// 路径相关配置
struct PathConfig {
    std::filesystem::path configFile;  ///< 配置文件路径
    std::filesystem::path dataRoot;    ///< 数据根目录
    std::filesystem::path ccfFile;     ///< CCF 文件路径
    std::filesystem::path kernelFile;  ///< 内核文件路径
};

/// 日志输出配置
struct LoggingConfig {
    bool enabled = true;
    std::filesystem::path filePath{"./logs/dss.log"};
    std::size_t maxFileSizeBytes = 10U * 1024U * 1024U;
    std::size_t maxFiles = 5U;
};

/// 通信与网络配置
struct CommNetConfig {
    SerialConfig displayPort;        ///< 显示串口
    SerialConfig exposurePort;       ///< 曝光同步串口
    SerialConfig masterControlPort;  ///< 主控串口
    SerialConfig servoPort;          ///< 伺服串口
    std::string cameraPort;          ///< 相机端口

    UdpEndpointConfig imageSender;   ///< 图像发送端点
    UdpEndpointConfig exchange;      ///< 旧版兼容数据交换端点
    UdpEndpointConfig exchangeGxtc;  ///< GXTC 数据交换端点
    UdpEndpointConfig exchangeGdcl;  ///< GDCL 数据交换端点
    UdpEndpointConfig errorDiag;     ///< 误差诊断端点
    UdpEndpointConfig atmos;         ///< 大气数据端点
    UdpEndpointConfig heartbeat;     ///< 心跳端点
};

/// 图像处理策略配置
struct ProcessingConfig {
    double thresholdSigma = 3.0;
    std::uint16_t diffThreshold = 20;
    int minArea = 3;
    int maxArea = 100000;
    std::uint16_t displayLow = 0;
    std::uint16_t displayHigh = 16384;
};

/// 台站地理坐标配置
struct ObservatoryConfig {
    std::string id{"0"};     ///< 台站编号
    double longitude = 0.0;  ///< 经度（度）
    double latitude = 0.0;   ///< 纬度（度）
    double altitude = 0.0;   ///< 海拔（米）
};

/// 全局配置单例，负责从 JSON 加载和持久化系统配置
class Config {
public:
    /// 获取全局唯一实例
    static auto instance() -> Config& {
        static Config config;
        return config;
    }

    /**
     * @brief 从文件加载配置
     * @param configPath JSON 配置文件路径
     * @return 失败时返回包含错误信息的 std::expected
     */
    auto load(const std::filesystem::path& configPath) -> std::expected<void, std::string>;

    /**
     * @brief 将当前配置持久化到文件
     * @return 失败时返回包含错误信息的 std::expected
     */
    auto save() -> std::expected<void, std::string>;

    /// 获取路径配置（只读）
    [[nodiscard]] auto paths() const -> const PathConfig& {
        return m_paths;
    }
    /// 获取日志配置（只读）
    [[nodiscard]] auto logging() const -> const LoggingConfig& {
        return m_logging;
    }
    /// 获取通信网络配置（只读）
    [[nodiscard]] auto commNet() const -> const CommNetConfig& {
        return m_commNet;
    }
    /// 获取光学参数（只读）
    [[nodiscard]] auto optics() const -> const OpticParams& {
        return m_optics;
    }
    /// 获取图像处理参数（只读）
    [[nodiscard]] auto processing() const -> const ProcessingConfig& {
        return m_processing;
    }
    /// 获取台站坐标（只读）
    [[nodiscard]] auto observatory() const -> const ObservatoryConfig& {
        return m_observatory;
    }
    /// 获取跟踪参数（只读）
    [[nodiscard]] auto trackingSettings() const -> const TrackingSettings& {
        return m_tracking;
    }

    /// 获取路径配置（可写）
    auto mutablePaths() -> PathConfig& {
        return m_paths;
    }
    /// 获取日志配置（可写）
    auto mutableLogging() -> LoggingConfig& {
        return m_logging;
    }
    /// 获取通信网络配置（可写）
    auto mutableCommNet() -> CommNetConfig& {
        return m_commNet;
    }
    /// 获取光学参数（可写）
    auto mutableOptics() -> OpticParams& {
        return m_optics;
    }
    /// 获取跟踪参数（可写）
    auto mutableTracking() -> TrackingSettings& {
        return m_tracking;
    }

private:
    Config() = default;

    PathConfig m_paths{};               ///< 路径配置
    LoggingConfig m_logging{};          ///< 日志配置
    CommNetConfig m_commNet{};          ///< 通信网络配置
    OpticParams m_optics{};             ///< 光学参数
    ProcessingConfig m_processing{};    ///< 图像处理参数
    ObservatoryConfig m_observatory{};  ///< 台站坐标
    TrackingSettings m_tracking{};      ///< 跟踪参数
};

}  // namespace Dss::Core
