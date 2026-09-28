#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Core {

/// 采集事件：采集源报告错误。
struct AcquisitionErrorEvent {
    std::string source;   ///< 报告错误的采集源名称
    std::string message;  ///< 错误描述
};

/// 系统事件：后台任务的未处理异常已被线程边界拦截。
struct BackgroundTaskErrorEvent {
    std::string component;  ///< 后台组件稳定名称
    std::string message;    ///< 异常描述
};

/// 采集事件：一帧图像已就绪。
struct FrameAcquiredEvent {
    uint64_t frameSeq = 0;           ///< 帧序号
    uint32_t width = 0;              ///< 图像宽度（像素）
    uint32_t height = 0;             ///< 图像高度（像素）
    ExposureDisplayData metadata{};  ///< 曝光与显示元数据
};

/// 采集事件：开始抓帧
struct GrabStartedEvent {
    uint32_t width = 0;   ///< 图像宽度（像素）
    uint32_t height = 0;  ///< 图像高度（像素）
};

/// 采集事件：停止抓帧
struct GrabStoppedEvent {};

/// 时间序列边界：旧帧已停止派发，消费者应丢弃尚未应用的 UI 更新。
struct ProcessingSessionResetEvent {};

/// 处理事件：请求刷新显示
struct DisplayRefreshEvent {
    uint64_t frameSeq = 0;                                     ///< 帧序号
    uint32_t width = 0;                                        ///< 图像宽度（像素）
    uint32_t height = 0;                                       ///< 图像高度（像素）
    uint32_t stride = 0;                                       ///< 行跨度（像素）
    std::shared_ptr<const std::vector<uint8_t>> displayImage;  ///< 显示用图像数据
    std::shared_ptr<const std::vector<uint16_t>> rawImage;     ///< 可用于实时重拉伸的 16 位原始图像
    ImageStats stats{};                                        ///< 当前 RAW 图像统计量
    uint16_t displayStretchLow = 0;                            ///< 当前帧实际使用的显示低阈值
    uint16_t displayStretchHigh = 1;                           ///< 当前帧实际使用的显示高阈值
    bool displayStretchWindowValid = false;                    ///< low/high 是否来自有效显示窗口
};

/// 处理事件：单帧处理完成
struct ProcessingCompleteEvent {
    uint64_t frameSeq = 0;  ///< 帧序号
    ImageStats stats{};     ///< 图像统计量
};

/// 处理事件：旋转校正后的帧已就绪
struct RotatedFrameReadyEvent {
    uint64_t frameSeq = 0;  ///< 帧序号
};

/// 跟踪事件：每次实际执行策略后发布，包括无目标的空列表。
/// GEO 失活当帧含一次最终快照，下帧移除；UI 仅展示 living 目标。
/// 最新测量 valid 与 living 独立，存储/网络继续按测量有效性处理最终快照。
struct TrackResultEvent {
    uint64_t frameSeq = 0;  ///< 帧序号
    std::vector<TargetInfo>
        targets;  ///< 当前目标快照；frameInfos 为近期窗口，累计长度见 totalFrameCount()。
};

/// 网络事件：处理完成的显示图像已可提交发送
struct ImageReadyForSendEvent {
    using ImageFactory =
        std::function<std::shared_ptr<const std::vector<uint8_t>>()>;  ///< 延迟 8 位图生成器

    uint64_t frameSeq = 0;                              ///< 帧序号
    uint32_t width = 0;                                 ///< 图像宽度（像素）
    uint32_t height = 0;                                ///< 图像高度（像素）
    std::shared_ptr<const std::vector<uint8_t>> image;  ///< 不可变 8 位灰度图像
    ImageFactory imageFactory;                          ///< 仅在消费者确实需要时执行的图像生成器
    std::size_t retainedSourceBytes = 0;  ///< factory 捕获的图像载荷容量，0 表示未计费。
};

/// 网络事件：图像的全部 UDP 分片已提交给套接字
struct ImageSendCompletedEvent {
    uint64_t frameSeq = 0;  ///< 已发送图像的帧序号
};

/// 网络事件：报文发送失败
struct NetworkTransmissionErrorEvent {
    std::string channel;          ///< 网络通道名称
    std::string message;          ///< 错误描述
    uint64_t attemptedBytes = 0;  ///< 尝试发送的字节数
};

/// 串口事件：接收帧校验失败
struct SerialFrameErrorEvent {
    std::string channel;         ///< 串口通道名称
    std::string message;         ///< 错误描述
    uint64_t expectedBytes = 0;  ///< 期望帧字节数
    uint64_t actualBytes = 0;    ///< 实际帧字节数
    uint8_t observedHeader = 0;  ///< 实际帧头字节
    uint8_t observedTail = 0;    ///< 实际帧尾字节
};

/// 串口事件：协议字段解码失败
struct SerialDecodeErrorEvent {
    std::string channel;      ///< 串口通道名称
    std::string message;      ///< 错误描述
    std::string field;        ///< 发生错误的协议字段名称
    uint64_t byteOffset = 0;  ///< 字段起始字节偏移
    uint64_t rawValue = 0;    ///< 字段原始值或解码后的异常值
};
/// 存储事件：后台写入失败
struct StorageWriteErrorEvent {
    std::string backend;  ///< 存储后端名称
    std::string path;     ///< 失败的目标路径
    std::string message;  ///< 文件系统或编码错误
};

/// 串口事件：主控指令
struct MasterControlEvent {
    float exposure = 0.0f;  ///< 曝光时间
    int trackMode = 0;      ///< 跟踪模式
    uint8_t mode1 = 0;      ///< 模式字节 1
    uint8_t mode2 = 0;      ///< 模式字节 2
    bool save = false;      ///< 是否保存数据
    bool grab = false;      ///< 是否抓帧
    bool track = false;     ///< 是否跟踪
    uint32_t targetId = 0;  ///< 目标编号
    uint32_t taskId = 0;    ///< 任务编号
    TimeOfDay start{};      ///< 任务开始时刻
    TimeOfDay end{};        ///< 任务结束时刻
};

/// 串口事件：曝光同步数据
struct ExposureSyncEvent {
    ExposureDisplayData data{};  ///< 曝光与显示同步数据
};

/// 串口事件：25 Hz 同步脉冲
struct Sync25HzEvent {};

/// UI 事件：手动选择目标
struct ManualTargetSelectEvent {
    float x = 0.0f;  ///< 点击 X 坐标（像素）
    float y = 0.0f;  ///< 点击 Y 坐标（像素）
};

/// UI 事件：缩放级别变更
struct ZoomChangeEvent {
    int level = 0;  ///< 缩放级别
};

/// UI 事件：关闭应用
struct CloseEvent {};

/// 系统日志级别
enum class LogLevel {
    Info = 0,     ///< 普通信息
    Warning = 1,  ///< 警告信息
    Error = 2,    ///< 错误信息
};

/// 系统事件：日志消息
struct LogMessageEvent {
    LogLevel level = LogLevel::Info;  ///< 日志级别
    std::string message;              ///< 日志文本
};

/// 系统事件：大气环境数据
struct AtmosphereDataEvent {
    double temperature = 0.0;  ///< 温度
    double pressure = 0.0;     ///< 气压
    double humidity = 0.0;     ///< 湿度
};

}  // namespace Dss::Core
