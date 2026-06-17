# ViewModel 模块化重构设计

## 目标

旧 `Dss::Ui::ViewModel` 曾同时承担回放、显示、处理、跟踪、存储、串口、网络、数据交换、日志与事件订阅职责，已经成为 UI 层的集中调度器。本次重构目标是在不破坏现有回放模式和硬件默认不启动策略的前提下，将 UI 状态与命令按业务域拆成多个小型 `QObject` 模块，使每个模块可以独立测试、独立演进，并让 `MainWindow` 直接依赖 `MainViewModel` 主 ViewModel 和对应子 ViewModel。

## 设计原则

- 保持渐进迁移：每次迁移一个页面或一组控件，迁移完成后补测试并运行相关 CTest。
- 保持硬件安全：串口和 UDP 服务仍由用户显式打开，配置编辑不得自动触碰真实硬件。
- 保持 UI 可用：`MainWindow` 通过 `MainViewModel` 获取子模块，旧 facade 不再参与新代码路径。
- 避免重复逻辑：服务查询、配置校验、状态文本生成等公共能力沉到轻量 helper 或基类中。
- 保持 Doxygen 中文注释：新增类、接口、成员和公开函数均添加中文 Doxygen 注释。

## 目标架构

`MainViewModel` 是 UI 层主 ViewModel 与轻量组合根，负责创建子模块、汇总状态文本、连接少量跨模块关系，并订阅需要 UI 级协调的主控事件。新增模块按职责拆分，并共享 `MessageBus`、`ServiceRegistry` 和配置对象引用。

```text
MainWindow
  ├─ MainViewModel（主 ViewModel / 组合根）
  ├─ ReplayViewModel
  ├─ DisplayViewModel
  ├─ ProcessingViewModel
  ├─ TrackingViewModel
  ├─ StorageViewModel
  ├─ SerialPortViewModel
  ├─ NetworkViewModel
  ├─ DataExchangeViewModel
  └─ LogViewModel

MainViewModel
  ├─ 创建并持有上述子模块
  ├─ 汇总子模块状态文本，转发给 MainWindow 状态栏
  └─ 协调 MasterControlEvent 对曝光、跟踪、保存和回放的影响
```

## 模块边界

### ReplayViewModel

负责图像序列回放入口，包括选择文件、初始化 `ImageSequenceFrameSource`、开始/停止、单帧步进、当前帧和总帧数状态。它只关心 `IFrameSource`/`ImageSequenceFrameSource`，不构造显示图、不直接控制处理策略。

### DisplayViewModel

负责显示相关状态，包括自动/手动拉伸参数、当前 raw frame 缓存、当前显示图刷新、图像统计、缩放级别和 `displayImageReady` 信号。它订阅 `DisplayRefreshEvent`，并在拉伸参数实时改变时基于缓存 raw frame 立即刷新当前显示图。

### ProcessingViewModel

负责处理模式与 `ImageProcessor` 策略同步。它只处理 CPU/OpenCV、关闭处理、后续 CUDA 策略选择等处理链路配置，不承担跟踪模式和手动目标选择。

### TrackingViewModel

负责跟踪模式、手动选点、跟踪状态文本和跟踪结果事件。Manual/GEO/LEO/SC 等策略选择与 `ImageProcessor` 的 tracking strategy 同步放在此模块，便于后续继续迁移 LEO/SC。

### StorageViewModel

负责保存开关、图像存储 worker、轨迹存储 worker 和保存状态。后续 BMP/IFM/会话索引、轨迹文本、背压策略和错误事件统一从这里扩展。

### SerialPortViewModel

负责四路串口通道的 UI 状态、配置编辑、打开/关闭和串口命令发送。它覆盖 `display`、`exposure`、`master_control`、`servo`，并封装 Exposure、Servo、MasterControl 命令入口。串口帧错误和解码错误只生成结构化日志事件或调用 `LogViewModel` 的日志入口，不与网络逻辑混放。

### NetworkViewModel

负责普通 UDP 网络端点的 UI 状态、配置编辑和打开/关闭，覆盖 `image_sender`、`error_diagnostics`、`atmos_receiver`、`heartbeat` 等 `INetworkChannel` 服务。网络发送失败状态和日志文本在该模块或日志 helper 中统一生成。

### DataExchangeViewModel

负责 GXTC/GDCL 数据交换端点配置、打开/关闭和状态同步。它依赖 `DataExchange` 服务，与普通 `INetworkChannel` 分开，避免双通道数据交换协议把 `NetworkViewModel` 重新撑大。

### LogViewModel

负责 UI 日志缓存、最小等级过滤、彩色显示所需的日志条目输出，以及 `LogMessageEvent`、网络发送错误、串口帧错误、串口解码错误的 UI 日志桥接。`MainWindow` 日志页最终只依赖该模块。

## 共享基础设施

新增轻量 `UiServiceContext` 保存以下引用：

- `Dss::Core::MessageBus& bus`
- `Dss::Core::ServiceRegistry& registry`

新增小型 helper，避免每个子模块重复写服务查询与配置转换：

- `service_lookup.*`：按名称获取服务，返回 `std::expected<std::shared_ptr<T>, QString>`。
- `endpoint_state_mapper.*`：网络端点、串口端点配置与 UI state 的转换。
- `ui_status_helpers.*`：统一生成常用状态文本和错误文本。

这些 helper 不持有 Qt 对象生命周期，只提供纯函数或轻量封装，方便单元测试。

## 迁移顺序

1. 建立 `UiServiceContext` 和子模块目录结构，不改变 UI 行为。
2. 迁移 `LogViewModel`，把日志缓存、过滤和错误事件文本从旧 `ViewModel` 拆出。
3. 迁移 `ReplayViewModel`，让回放页开始直接依赖新模块。
4. 迁移 `DisplayViewModel`，把实时拉伸和当前帧缓存从旧 `ViewModel` 拆出。
5. 迁移 `ProcessingViewModel` 与 `TrackingViewModel`，拆开处理策略与跟踪策略。
6. 迁移 `StorageViewModel`，集中保存开关与存储 worker 控制。
7. 迁移 `SerialPortViewModel`，串口配置、打开关闭和命令发送从旧 `ViewModel` 中移除。
8. 迁移 `NetworkViewModel`，普通 UDP 端点配置和打开关闭从旧 `ViewModel` 中移除。
9. 迁移 `DataExchangeViewModel`，GXTC/GDCL 独立成模块。
10. 清理旧 `ViewModel` 的兼容接口，完成后删除旧 Facade，由 `MainViewModel` 作为 UI 层主 ViewModel 与唯一组合根。

## MainWindow 迁移策略

`MainWindow` 构造期接收 `MainViewModel`，并通过访问器取得子模块：

```cpp
auto& replay = m_mainViewModel.replay();
auto& display = m_mainViewModel.display();
auto& serial = m_mainViewModel.serialPorts();
auto& network = m_mainViewModel.network();
```

每个页面的信号槽连接都应落到对应子模块。跨模块协调只放在 `MainViewModel` 或事件总线中，避免子模块互相持有导致新的单体化。

## 测试策略

- 每个子模块新增独立测试文件，例如 `test_replay_view_model.cpp`、`test_display_view_model.cpp`、`test_serial_port_view_model.cpp`。
- 旧测试按迁移节奏拆分，确保每个测试只覆盖一个业务域。
- 保留端到端 UI 行为测试，例如 `test_main_window_layout`；新增 `test_main_view_model` 覆盖主 ViewModel 状态汇总和主控协调。
- 串口和网络测试继续使用 fake/stub 服务，验证默认不开硬件、配置变更不自动打开、打开失败能反馈状态。
- 每阶段至少运行对应模块测试和相关 `MainWindow`/回放/网络测试。

## 风险与控制

- 风险：一次迁移过多信号槽导致 UI 行为回归。控制：按页面和业务域拆分，每阶段小步提交。
- 风险：子模块之间直接互调造成新的耦合。控制：跨模块状态优先通过事件总线或组合根协调，避免模块互相持有。
- 风险：串口和网络配置逻辑重复。控制：公共校验和 state mapper 作为纯 helper 复用。
- 风险：主 ViewModel 重新膨胀成新的单体。控制：`MainViewModel` 只做子模块创建、状态汇总和跨模块事件协调，业务命令继续归属具体子 ViewModel。

## 完成标准

- 旧 `ViewModel` facade 已从代码和构建中移除。
- `MainWindow` 通过 `MainViewModel` 直接依赖对应子模块。
- 串口与网络模块独立，DataExchange 作为网络协议特化模块独立存在。
- 新增和迁移后的公开接口均具备中文 Doxygen 注释。
- 相关单元测试和现有 UI/服务测试通过。
