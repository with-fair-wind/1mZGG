# Processing 模块 (`dss_processing` / `dss_processing_opencv`)

> 命名空间: `Dss::Processing`
>
> 头文件: `include/dss/processing/`
>
> 源文件: `src/processing/`
>
> 依赖: `dss_core`, `dss_tracking`; 可选 `OpenCV`

## 模块职责

Processing 模块实现图像处理管线，负责帧数据的缓冲、分发、统计分析、阈值分割、连通域检测和目标提取。通过策略模式支持多种处理后端（CPU/OpenCV/CUDA）。

## 组件清单

### 1. FramePacket (`frame_packet.h`)

多缓冲区帧数据模型，承载一帧图像从采集到处理完成的全部数据。

| 字段 | 类型 | 用途 |
|------|------|------|
| `rawBuffer` | `vector<uint16_t>` | 原始 16 位图像 |
| `rotatedBuffer` | `vector<uint16_t>` | 旋转后图像 |
| `displayBuffer` | `vector<uint8_t>` | 8 位显示图像 |
| `photometryBuffer` | `vector<uint8_t>` | 光度测量图像 |
| `blobs` | `vector<MeasuredBlob>` | 检测到的目标列表 |
| `width` / `height` | `uint32_t` | 图像尺寸 |
| `frameSeq` | `uint64_t` | 帧序号 |
| `metadata` | `ExposureDisplayData` | 曝光同步元数据 |

### 2. FrameView / MutableFrameView (`frame_view.h`)

帧数据的非拥有视图（基于 `std::span`），提供零拷贝访问。

- `makeFrameView(packet)` → 只读视图
- `makeMutableFrameView(packet)` → 可变视图

### 3. BoundedChannel (`bounded_channel.h`)

线程安全的有界队列，用于生产者-消费者模式的帧传递。

```cpp
BoundedChannel<FramePacket, 4>  // 容量为 4 的帧缓冲
```

- `push(item)` — 入队，满则丢弃最老帧
- `pop(stop_token)` — 阻塞出队，支持协作式取消
- `close()` — 关闭通道

### 4. IProcessingStrategy (`i_processing_strategy.h`)

可插拔的图像处理后端接口。

```cpp
class IProcessingStrategy {
    virtual auto process(const FramePacket& input) -> ProcessingResult = 0;
    virtual auto name() const -> std::string_view = 0;
    virtual auto mode() const -> ProcessingMode = 0;
};
```

### 5. ProcessingPipeline (`processing_pipeline.h`)

处理管线调度器，持有当前后端实例并委托执行。

| 方法 | 说明 |
|------|------|
| `setBackend(strategy)` | 切换处理后端 |
| `process(packet)` | 执行当前后端的处理，返回 `ProcessingResult` |
| `currentMode()` / `backendName()` | 查询当前后端模式和名称 |
| `hasBackend()` | 判断是否启用了处理后端 |

### 6. ImageProcessor (`image_processor.h`)

核心帧处理工作线程：

1. 从 `BoundedChannel` 取帧
2. 调用 `ProcessingPipeline::process()` 执行图像处理
3. 调用 `ITrackingStrategy::track()` 执行跟踪（如已设置）
4. 在消息总线上发布 `DisplayRefreshEvent`、`ProcessingCompleteEvent` 等事件

| 方法 | 说明 |
|------|------|
| `start()` | 启动工作线程 (`std::jthread`) |
| `stop()` | 停止工作线程 |
| `submitFrame(packet)` | 提交帧到处理队列 |
| `droppedFrames()` | 获取丢帧计数 |
| `setProcessingStrategy()` | 切换处理后端 |
| `setTrackingStrategy()` | 切换跟踪策略 |
| `currentProcessingMode()` / `currentTrackMode()` | 查询当前处理/跟踪模式 |

无处理 backend 时，`ImageProcessor` 仍会把原始回放帧作为 Direct 帧发布显示；如果当前跟踪策略是 Manual 且已有 UI 选点，也会构造 `FrameMeasurements` 并发布 `TrackResultEvent`。这使无 Sapera、无 OpenCV/CUDA 策略的回放模式也能验证“选序列 → 显示 → 手动跟踪”的主链路。

### 7. Labeler (`labeler.h`)

CPU 连通域检测与目标提取。

```cpp
auto labelAndExtract(span<const uint8_t> binaryImage,
                     uint32_t width, uint32_t height)
    -> vector<MeasuredBlob>;
```

从二值图像中检测连通区域并计算质心、边界框、面积、灰度总和等。

### 8. OpenCvProcessingStrategy (`opencv_processing_strategy.h`)

基于 OpenCV 的参考处理后端（可选 target `dss_processing_opencv`）：

- 统计分析（最大/最小/均值/标准差）
- 16 位 → 8 位映射（线性拉伸）
- 自适应阈值分割
- 连通域检测 (`cv::connectedComponentsWithStats`)

## 处理流水线

```
原始帧 (16-bit)
    │
    ▼
统计分析 (max/min/avg/σ)
    │
    ▼
16→8 位映射 (线性拉伸)
    │
    ▼
阈值分割 → 二值图
    │
    ▼
连通域检测 → MeasuredBlob[]
    │
    ▼
跟踪算法 → TargetInfo[]
    │
    ▼
事件发布 (DisplayRefresh / ProcessingComplete / TrackResult)
```

## 当前缺口

| 缺口 | 说明 |
|------|------|
| GPU 后端未接入 | CUDA 核函数已移植但未作为 `IProcessingStrategy` 实现 |
| 帧差法未实现 | `ProcessingMode::Diff` 仅定义枚举，无实现 |
| 光度测量未接入 | `photometryBuffer` 未使用 |
| 星图匹配未接入 | `DSS_ENABLE_STARLIBS` 接口未实现 |
| 完整处理策略命令 | UI 已支持 None/OpenCV 切换；OpenCV 参数、Diff 模式和 CUDA 模式尚未接入 |

## 依赖关系

```
dss_processing
├── dss_core
└── dss_tracking

dss_processing_opencv (可选)
├── dss_processing
└── OpenCV (core, imgproc)
```
