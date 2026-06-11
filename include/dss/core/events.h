#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Core {

/// 采集事件：一帧图像已就绪
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

/// 处理事件：请求刷新显示
struct DisplayRefreshEvent {
    uint64_t frameSeq = 0;                                     ///< 帧序号
    uint32_t width = 0;                                        ///< 图像宽度（像素）
    uint32_t height = 0;                                       ///< 图像高度（像素）
    uint32_t stride = 0;                                       ///< 行跨度（字节）
    std::shared_ptr<const std::vector<uint8_t>> displayImage;  ///< 显示用图像数据
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

/// 跟踪事件：跟踪结果更新
struct TrackResultEvent {
    uint64_t frameSeq = 0;            ///< 帧序号
    std::vector<TargetInfo> targets;  ///< 当前跟踪目标列表
};

/// 网络事件：请求发送图像
struct ImageSendEvent {
    uint64_t frameSeq = 0;  ///< 帧序号
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

/// 系统事件：日志消息
struct LogMessageEvent {
    std::string message;  ///< 日志文本
};

/// 系统事件：大气环境数据
struct AtmosphereDataEvent {
    double temperature = 0.0;  ///< 温度
    double pressure = 0.0;     ///< 气压
    double humidity = 0.0;     ///< 湿度
};

}  // namespace Dss::Core
