# Core 模块 (`dss_core`)

> 命名空间: `Dss::Core`、`Dss::Evt`
>
> 头文件: `include/dss/core/`
>
> 源文件: `src/core/`
>
> 依赖: `nlohmann_json`

## 模块职责

Core 模块是整个系统的基础层，提供所有其他模块共享的类型定义、事件系统、配置管理和服务基础设施。该模块完全不依赖 Qt，可独立编译和测试。

## 组件清单

### 1. 类型系统 (`types.h`)

定义全局共享的领域数据结构，所有结构体均为纯数据对象 (POD-like)。

| 类型 | 用途 |
|------|------|
| `Timestamp` | 时间戳 (年月日时分秒毫秒微秒) |
| `TimeOfDay` | 时分秒 |
| `OpticParams` | 光学参数 (图像尺寸、视场中心、像元比例) |
| `Vec2f` / `Vec2d` | 二维向量 (float/double) |
| `MeasuredBlob` | 单个目标检测结果 (质心、边界、灰度、面积、方位角/俯仰角等) |
| `FrameMeasurements` | 单帧全部检测结果 (时间戳、序号、所有目标/星体) |
| `TargetFrameInfo` | 单帧中单个目标的状态 |
| `TargetInfo` | 跨帧的目标跟踪状态 (历史帧信息、预测位置/速度) |
| `ImageStats` | 图像统计量 (最大/最小/均值/标准差) |
| `TrackingSettings` | 跟踪参数 (搜索半径、活性阈值、GEO FullLEO/RA-Dec 阈值等) |
| `ExposureDisplayData` | 曝光/显示同步数据 |
| `ResultPacket` | 测量结果数据包，作为 UI、存储和 GXTC/GDCL 映射的共享 DTO，包含目标位置、角速度、光度和环境数据 |
| `PointingErrorResult` | 指向误差模型参数 |

### 1.1 结果包工具 (`result_packet_utils.h`)

将跟踪策略输出的 `TargetInfo` 归一为 `ResultPacket`，避免 UI、存储、网络分别读取 `frameInfos.back()` 并重复拼字段。当前结果包会带出最新帧测量、轴系/天文定位、预测角速度和星等，供轨迹文本和 GXTC/GDCL 协议 adapter 复用。

| 函数 | 说明 |
|------|------|
| `latestTargetFrameInfo(target)` | 获取目标轨迹最新帧，空轨迹返回 `nullptr` |
| `makeResultPacket(target)` | 从最新帧构造通用测量结果数据包，空轨迹返回空 |
| `makeResultPackets(targets)` | 批量构造结果包并跳过空轨迹 |

### 2. 常量 (`constants.h`)

类型安全的枚举和编译期常量，替代旧版 `DefinedMacro.h` 中的宏定义。

**枚举类型:**

| 枚举 | 值 | 用途 |
|------|-----|------|
| `InitStatus` | Ok, ErrorCommNetSettings, ErrorPath, ... | 初始化状态 |
| `Status` | Init, Error, Ok | 三态运行状态 |
| `TriggerMode` | External, Free | 相机触发模式 |
| `CommDisplayMode` | PortInit, Recv, Send, RecvCheck | 串口显示模式 |
| `ProcessingMode` | None, Diff, Direct | 图像处理模式 |
| `TrackMode` | Init, Geo, SpaceCatalog, Leo, Manual | 跟踪模式 |

**天文常量:**
- `Pi`, `DegToRad`, `RadToDeg`, `ArcSecToRad`, `RadToArcSec`, `SecToRad`, `SolarSiderealRatio`

**协议常量:**
- `FrameHeader` (0x7E), `FrameTail` (0xE7)

### 3. 事件系统 (`events.h`)

定义在 `BasicMessageBus` 上传播的类型化事件载荷。

| 事件 | 生产者 | 消费者 |
|------|--------|--------|
| `FrameAcquiredEvent` | Grabber | ImageProcessor |
| `GrabStartedEvent` / `GrabStoppedEvent` | Grabber | ViewModel |
| `DisplayRefreshEvent` | ImageProcessor | ViewModel → ImageDisplay |
| `ProcessingCompleteEvent` | ImageProcessor | ViewModel |
| `RotatedFrameReadyEvent` | ImageProcessor | Storage |
| `TrackResultEvent` | TrackManager | ViewModel, ServoChannel |
| `ImageSendEvent` | ImageSender | — |
| `NetworkTransmissionErrorEvent` | DataExchange | ViewModel/Logger |
| `MasterControlEvent` | MasterControlChannel | ViewModel |
| `ExposureSyncEvent` | ExposureChannel | ImageProcessor |
| `Sync25HzEvent` | DisplayChannel | ImageProcessor |
| `ManualTargetSelectEvent` | UI | ManualTracker |
| `ZoomChangeEvent` | UI | ImageDisplay |
| `CloseEvent` | UI | ApplicationContext |
| `LogMessageEvent` | Logger | UI Log Panel |
| `AtmosphereDataEvent` | AtmosReceiver | — |

### 4. 事件总线 (`event_bus.h`)

命名空间 `Dss::Evt`，从旧版 `eventBus17.hpp` 移植并增强。

**核心组件:**

- **`Event<Signature, Combiner, LockPolicy>`** — 类型化事件，支持 void 和有返回值两种特化
  - `subscribe()` → 返回 `ScopedConnection`（RAII 自动断连）
  - `emit()` — 同步分发给所有处理器
  - `post()` / `flush()` — 延迟分发（先入队，后批量触发）
- **`BasicMessageBus<LockPolicy>`** — 基于 `type_index` 的消息总线
  - `subscribe<Message>()` — 按类型订阅
  - `emit<Message>()` — 按类型分发
  - `post()` / `flush()` — 延迟分发
- **`ScopedConnection`** — RAII 连接管理器，析构时自动断开订阅
- **`Delegate<R(Args...)>`** — 类型擦除的可调用对象包装

**锁策略:**
- `NoLock` — 单线程，零开销
- `SharedMutexLock` — 多线程安全（读写锁）

**Copy-on-Write:** 处理器列表使用 COW 快照模式，`emit()` 执行时持有快照的共享指针，确保分发期间可安全修改订阅列表。

### 5. 配置系统 (`config.h` / `config_types.h`)

取代旧版 `GlobalParameter` + `QSettings`/INI 方案，使用 JSON 格式。

**`Config` 类 (单例):**
- `load(path)` — 从 JSON 文件加载配置
- `save(path)` — 保存到 JSON 文件
- 分域访问器: `optics()`, `comm()`, `tracking()`, `paths()` 等
- 可变访问器: `mutableOptics()` 等

**配置 DTO:**
- `SerialConfig` — 串口配置 (端口名、波特率)
- `UdpEndpointConfig` — UDP 端点配置 (地址、端口)
- `CommNetConfig::exchangeGxtc/exchangeGdcl` — GXTC/GDCL 独立数据交换端点；旧 `exchange` 配置仍会兼容推导

### 6. 服务基础设施

**`IService`** (`i_service.h`) — 服务生命周期接口:
- `name()` → 服务名称
- `start()` → 启动
- `stop()` → 停止

**`ServiceHost`** (`service_host.h`) — 有序服务启停:
- `add(service)` — 注册服务
- `startAll()` — 按注册顺序启动
- `stopAll()` — 按逆序停止

**`ServiceRegistry`** (`service_registry.h`) — 按类型索引的服务查找:
- `registerService<T>(name, instance)` — 注册
- `get<T>(name)` — 获取（不存在则抛异常）
- `tryGet<T>(name)` — 获取（不存在返回 nullptr）

### 7. 日志 (`logger.h`)

基于事件总线的日志系统，替代旧版 `MyLog`。

- `Logger::info()`, `warn()`, `error()` — 写日志
- 内部发布 `LogMessageEvent` 到消息总线
- 由 `ApplicationContext::wireLogger()` 初始化连接

## 依赖关系

```
dss_core
└── nlohmann_json::nlohmann_json
```

Core 模块是所有其他模块的基础依赖，不依赖 Qt 或任何其他项目模块。
