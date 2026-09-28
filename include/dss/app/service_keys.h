#pragma once

#include <string_view>

namespace Dss::App::ServiceKey {

/** @brief 显示串口服务名。 */
inline constexpr std::string_view display = "display";
/** @brief 曝光串口和曝光命令服务名。 */
inline constexpr std::string_view exposure = "exposure";
/** @brief 主控串口和主控状态命令服务名。 */
inline constexpr std::string_view masterControl = "master_control";
/** @brief 伺服串口和伺服修正命令服务名。 */
inline constexpr std::string_view servo = "servo";
/** @brief 图像发送网络服务名。 */
inline constexpr std::string_view imageSender = "image_sender";
/** @brief 心跳网络服务名。 */
inline constexpr std::string_view heartbeat = "heartbeat";
/** @brief 错误诊断网络服务名。 */
inline constexpr std::string_view errorDiagnostics = "error_diagnostics";
/** @brief 数据交换网络服务名。 */
inline constexpr std::string_view dataExchange = "data_exchange";
/** @brief 跟踪结果到数据交换桥接服务名。 */
inline constexpr std::string_view trackResultDataExchangeBridge =
    "track_result_data_exchange_bridge";
/** @brief 大气接收网络服务名。 */
inline constexpr std::string_view atmosReceiver = "atmos_receiver";
/** @brief 相机控制服务名。 */
inline constexpr std::string_view camera = "camera";
/** @brief Sapera 实时帧源服务名。 */
inline constexpr std::string_view saperaSource = "sapera_source";
/** @brief 运行诊断服务名。 */
inline constexpr std::string_view runtimeDiagnostics = "runtime_diagnostics";
/** @brief 图像处理服务名。 */
inline constexpr std::string_view imageProcessor = "image_processor";
/** @brief 帧源协调器服务名。 */
inline constexpr std::string_view frameSource = "frame_source";
/** @brief 回放序列帧源服务名。 */
inline constexpr std::string_view replaySource = "replay_source";
/** @brief 回放生命周期编排服务名。 */
inline constexpr std::string_view replaySession = "replay_session";
/** @brief 本地图像存储服务名。 */
inline constexpr std::string_view imageStorage = "image_storage";
/** @brief 跟踪数据存储服务名。 */
inline constexpr std::string_view trackDataStorage = "track_data_storage";

}  // namespace Dss::App::ServiceKey
