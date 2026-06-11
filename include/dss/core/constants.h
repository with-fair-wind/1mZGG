#pragma once

#include <cstdint>
#include <numbers>

namespace Dss::Core {

/// 初始化配置文件读取状态
enum class InitStatus : int {
    Ok = 1,                     ///< 读取成功
    ErrorCommNetSettings = 0,   ///< 通信网络配置错误
    ErrorPath = -1,             ///< 路径配置错误
    ErrorCameraSettings = -2,   ///< 相机配置错误
    ErrorDisplaySettings = -3,  ///< 显示配置错误
    ErrorTrackSettings = -4,    ///< 跟踪配置错误
    ErrorOpticSettings = -5,    ///< 光学配置错误
    ErrorFileInvalid = -6,      ///< 配置文件无效
};

/// 三态运行状态
enum class Status : int {
    Init = -1,  ///< 初始化中
    Error = 0,  ///< 错误
    Ok = 1,     ///< 正常
};

/// 相机触发模式
enum class TriggerMode : int {
    External = 0,  ///< 外部触发
    Free = 1,      ///< 自由运行
};

/// 串口显示模式
enum class CommDisplayMode : int {
    PortInit = 0,   ///< 端口初始化
    Recv = 1,       ///< 接收
    Send = 2,       ///< 发送
    RecvCheck = 3,  ///< 接收校验
};

/// 图像处理模式
enum class ProcessingMode : int {
    None = 0,    ///< 不处理
    Diff = 1,    ///< 差分处理
    Direct = 3,  ///< 直接处理
};

/// 跟踪模式
enum class TrackMode : int {
    Init = -1,         ///< 未初始化
    Geo = 0,           ///< 地球同步轨道
    SpaceCatalog = 3,  ///< 空间编目
    Leo = 4,           ///< 低轨
    Manual = 5,        ///< 手动
};

/// 显示放大比例（原图尺寸的百分比 × 100）
inline constexpr int BiggerFirst = 36;   ///< 第一级放大比例
inline constexpr int BiggerSecond = 21;  ///< 第二级放大比例

/// 天文常量
inline constexpr double Pi = std::numbers::pi;                ///< 圆周率 π
inline constexpr double DegToRad = Pi / 180.0;                ///< 度转弧度
inline constexpr double RadToDeg = 180.0 / Pi;                ///< 弧度转度
inline constexpr double ArcSecToRad = Pi / (180.0 * 3600.0);  ///< 角秒转弧度
inline constexpr double RadToArcSec = (180.0 * 3600.0) / Pi;  ///< 弧度转角秒
inline constexpr double SecToRad = Pi / (12.0 * 3600.0);      ///< 时间秒转弧度（恒星时）
inline constexpr double SolarSiderealRatio = 1.00273790935;   ///< 太阳时与恒星时比值

/// 串口协议帧定界符
inline constexpr uint8_t FrameHeader = 0x7E;  ///< 帧头
inline constexpr uint8_t FrameTail = 0xE7;    ///< 帧尾

}  // namespace Dss::Core
