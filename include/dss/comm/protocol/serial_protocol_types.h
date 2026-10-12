#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "dss/comm/protocol/frame_codec.h"
#include "dss/core/types.h"

namespace Dss::Comm {

/// 串口通信协议类型
// 保留公共枚举的既有整数表示和对象布局。
// NOLINTNEXTLINE(performance-enum-size)
enum class SerialProtocol {
    Display,        ///< 显示通道协议
    Exposure,       ///< 曝光通道协议
    MasterControl,  ///< 主控通道协议
    Servo,          ///< 伺服通道协议
};

/// 各协议帧的布局描述
struct SerialFrameLayout {
    SerialProtocol protocol;   ///< 协议类型
    std::string_view name;     ///< 协议名称（用于错误信息）
    std::size_t recvSize = 0;  ///< 接收帧字节长度
    std::size_t sendSize = 0;  ///< 发送帧字节长度
};

/// 四种协议帧布局的查找表
inline constexpr std::array<SerialFrameLayout, 4> SerialFrameLayouts{
    SerialFrameLayout{SerialProtocol::Display, "display", 20, 9},
    SerialFrameLayout{SerialProtocol::Exposure, "exposure", 23, 8},
    SerialFrameLayout{SerialProtocol::MasterControl, "master_control", 30, 28},
    SerialFrameLayout{SerialProtocol::Servo, "servo", 20, 14},
};

/**
 * @brief 根据协议类型查找帧布局
 * @param protocol 协议类型
 * @return 匹配的帧布局；未找到时返回空布局
 */
[[nodiscard]] constexpr auto layoutFor(SerialProtocol protocol) -> SerialFrameLayout {
    for (const auto& layout : SerialFrameLayouts) {
        if (layout.protocol == protocol) {
            return layout;
        }
    }
    return {};
}

/**
 * @brief 校验接收帧的头尾与长度
 * @param protocol 协议类型
 * @param frame 待校验的帧数据
 * @return 帧合法时返回 true
 */
[[nodiscard]] inline bool validateReceiveFrame(SerialProtocol protocol,
                                               std::span<const uint8_t> frame) {
    return FrameCodec::validate(frame, layoutFor(protocol).recvSize);
}

/**
 * @brief 为发送帧写入帧头尾
 * @param protocol 协议类型
 * @param frame 待封装的帧缓冲区
 * @return 缓冲区长度匹配时返回 true
 */
inline bool prepareSendFrame(SerialProtocol protocol, std::span<uint8_t> frame) {
    if (frame.size() != layoutFor(protocol).sendSize) {
        return false;
    }
    FrameCodec::wrap(frame);
    return true;
}

/// 曝光触发模式
// 保留公共枚举的既有整数表示和对象布局。
// NOLINTNEXTLINE(performance-enum-size)
enum class ExposureTriggerMode {
    External,  ///< 外部触发
    FreeRun,   ///< 自由运行
};

/// 曝光通道发送命令参数
struct ExposureCommand {
    ExposureTriggerMode triggerMode = ExposureTriggerMode::External;  ///< 触发模式
    uint8_t frameFrequencyCode = 0x01;                                ///< 帧频编码
    uint32_t exposureDelayTicks = 0;                                  ///< 曝光延迟（时钟节拍）
};

/// 主控通道接收到的指令
struct MasterControlCommand {
    float exposure = 0.0F;         ///< 曝光时间
    uint8_t mode1 = 0;             ///< 主控模式字 1
    uint8_t mode2 = 0;             ///< 主控模式字 2
    int trackMode = -1;            ///< 跟踪模式（由 mode1/mode2 映射）
    bool track = false;            ///< 是否跟踪
    bool save = false;             ///< 是否保存
    bool grab = false;             ///< 是否抓取
    uint32_t targetId = 0;         ///< 目标编号
    uint32_t taskId = 0;           ///< 任务编号
    Dss::Core::TimeOfDay start{};  ///< 任务开始时刻
    Dss::Core::TimeOfDay end{};    ///< 任务结束时刻
};

/// 伺服修正量
struct ServoCorrection {
    bool distanceValid = false;            ///< 距离修正量是否有效
    bool speedValid = false;               ///< 速度修正量是否有效
    Dss::Core::Vec2f distanceArcsec{};     ///< 距离修正（角秒）
    Dss::Core::Vec2f speedArcsecPerSec{};  ///< 速度修正（角秒/秒）
    uint8_t mode = 0x19;                   ///< 伺服工作模式
};

/// 主控通道发送的状态帧内容
struct MasterControlStatus {
    ServoCorrection correction{};      ///< 伺服修正量
    Dss::Core::Timestamp timestamp{};  ///< 时间戳
    Dss::Core::Vec2f pointingAe{};     ///< 指向角（方位/俯仰，度）
};

/// 串口协议字段级解码错误
struct SerialDecodeError {
    std::string message;         ///< 可展示的错误描述
    std::string field;           ///< 发生错误的协议字段名称
    std::size_t byteOffset = 0;  ///< 字段起始字节偏移
    uint64_t rawValue = 0;       ///< 字段原始值或解码后的异常值
};

}  // namespace Dss::Comm
