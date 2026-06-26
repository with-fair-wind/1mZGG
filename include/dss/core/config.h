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
    bool enabled = true;                                 ///< 是否启用文件日志
    std::filesystem::path filePath{"./logs/dss.log"};    ///< 日志文件路径
    std::size_t maxFileSizeBytes = 10U * 1024U * 1024U;  ///< 单个日志文件的最大字节数
    std::size_t maxFiles = 5U;                           ///< 保留的轮转日志文件数量
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
    double thresholdSigma = 3.0;        ///< 目标检测阈值的标准差倍数
    std::uint16_t diffThreshold = 20;   ///< 帧差检测的灰度阈值
    int minArea = 3;                    ///< 有效目标的最小像素面积
    int maxArea = 100000;               ///< 有效目标的最大像素面积
    std::uint16_t displayLow = 0;       ///< 显示拉伸的输入下限
    std::uint16_t displayHigh = 16384;  ///< 显示拉伸的输入上限
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
    /**
     * @brief 获取全局唯一配置实例。
     * @return 全局配置实例的引用。
     */
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

    /** @brief 获取只读路径配置。 @return 路径配置的常量引用。 */
    [[nodiscard]] auto paths() const -> const PathConfig& {
        return m_paths;
    }
    /** @brief 获取只读日志配置。 @return 日志配置的常量引用。 */
    [[nodiscard]] auto logging() const -> const LoggingConfig& {
        return m_logging;
    }
    /** @brief 获取只读通信网络配置。 @return 通信网络配置的常量引用。 */
    [[nodiscard]] auto commNet() const -> const CommNetConfig& {
        return m_commNet;
    }
    /** @brief 获取只读光学参数。 @return 光学参数的常量引用。 */
    [[nodiscard]] auto optics() const -> const OpticParams& {
        return m_optics;
    }
    /** @brief 获取只读图像处理参数。 @return 图像处理配置的常量引用。 */
    [[nodiscard]] auto processing() const -> const ProcessingConfig& {
        return m_processing;
    }
    /** @brief 获取只读台站坐标。 @return 台站坐标配置的常量引用。 */
    [[nodiscard]] auto observatory() const -> const ObservatoryConfig& {
        return m_observatory;
    }
    /** @brief 获取只读跟踪参数。 @return 跟踪配置的常量引用。 */
    [[nodiscard]] auto trackingSettings() const -> const TrackingSettings& {
        return m_tracking;
    }

    /** @brief 获取可写路径配置。 @return 路径配置的引用。 */
    auto mutablePaths() -> PathConfig& {
        return m_paths;
    }
    /** @brief 获取可写日志配置。 @return 日志配置的引用。 */
    auto mutableLogging() -> LoggingConfig& {
        return m_logging;
    }
    /** @brief 获取可写通信网络配置。 @return 通信网络配置的引用。 */
    auto mutableCommNet() -> CommNetConfig& {
        return m_commNet;
    }
    /** @brief 获取可写光学参数。 @return 光学参数的引用。 */
    auto mutableOptics() -> OpticParams& {
        return m_optics;
    }
    /** @brief 获取可写跟踪参数。 @return 跟踪配置的引用。 */
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
