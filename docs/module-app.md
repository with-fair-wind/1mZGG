# App 模块 (`dss_app`)

> 命名空间: `Dss::App`
>
> 头文件: `include/dss/app/`
>
> 源文件: `src/app/`
>
> 依赖: `dss_core`, `dss_processing`, `dss_acquisition_qt`, `dss_comm_qt`, `dss_network_qt`

## 模块职责

App 模块是系统的**组合根 (Composition Root)**，负责组装所有子系统、管理全局生命周期、注册通信服务。它是 `main.cpp` 与各业务模块之间的中间层。

## 组件清单

### ApplicationContext

应用上下文，拥有并管理系统的三大基础设施：

| 成员 | 类型 | 职责 |
|------|------|------|
| `m_bus` | `BasicMessageBus<SharedMutexLock>` | 全局事件总线 |
| `m_registry` | `ServiceRegistry` | 服务注册中心 |
| `m_services` | `ServiceHost` | 服务启停编排器 |

**公共 API:**

| 方法 | 说明 |
|------|------|
| `bus()` | 获取消息总线引用 |
| `registry()` | 获取服务注册中心引用 |
| `services()` | 获取服务主机引用 |
| `wireLogger()` | 连接 spdlog 日志门面与事件总线 |
| `loadConfig(path)` | 加载 JSON 配置文件，返回 `std::expected` |
| `registerCommunicationServices()` | 注册所有串口/网络/存储/相机服务 |
| `startServices()` | 按序启动所有已注册服务 |
| `stopServices()` | 按逆序停止所有服务 |

### 通信服务注册 (`communication_services.cpp`)

`registerCommunicationServices()` 内部创建并注册以下服务实例。注册后默认不打开串口、不绑定 UDP、不触碰真实硬件；回放源、处理器和存储 worker 由 UI 命令显式启动。

| 服务 | 模块 | 注册名 |
|------|------|--------|
| `DisplayChannel` / `ISerialChannel` | dss_comm_qt | `display` |
| `ExposureChannel` / `ISerialChannel` / `IExposureCommandPort` | dss_comm_qt | `exposure` |
| `MasterControlChannel` / `ISerialChannel` / `IMasterControlStatusPort` | dss_comm_qt | `master_control` |
| `ServoChannel` / `ISerialChannel` / `IServoCorrectionPort` | dss_comm_qt | `servo` |
| `ImageSender` / `INetworkChannel` | dss_network_qt | `image_sender` |
| `Heartbeat` / `INetworkChannel` | dss_network_qt | `heartbeat` |
| `ErrorDiagnostics` / `INetworkChannel` | dss_network_qt | `error_diagnostics` |
| `DataExchange` | dss_network_qt | `data_exchange` |
| `TrackResultDataExchangeBridge` | dss_app | `track_result_data_exchange_bridge` |
| `AtmosReceiver` / `INetworkChannel` | dss_network_qt | `atmos_receiver` |
| `RuntimeDiagnostics` | dss_app | `runtime_diagnostics` |
| `ImageProcessor` | dss_processing | `image_processor` |
| `FrameSourceCoordinator` / `IFrameSource` | dss_acquisition_qt | `frame_source` |
| `ImageSequenceFrameSource` / `IFrameSource` | dss_acquisition_qt | `replay_source` |
| `SaperaFrameSource` / `IFrameSource`（条件注册） | dss_acquisition_qt | `sapera_source` |
| `LocalImageStorageBackend` / `IStorageBackend` | dss_core (`Dss::Storage`) | `image_storage` |
| `TrackDataStorageBackend` / `IStorageBackend` | dss_core (`Dss::Storage`) | `track_data_storage` |
| `CommandOnlyCameraController` | dss_acquisition_qt | `camera` |

## 启动流程 (`main.cpp`)

```
1. QApplication 初始化
2. ApplicationContext 构造
3. wireLogger()           → spdlog 日志事件转发就绪
4. loadConfig()           → JSON 配置加载
5. InitDialog 显示        → 初始化进度展示
6. registerCommunicationServices()  → 服务注册
7. MainViewModel 构造     → 创建子 ViewModel 并订阅事件
8. MainWindow 构造/显示   → UI 就绪
9. QApplication::exec()  → 事件循环
```

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 相机串口运行时接线 | 默认注册 `CommandOnlyCameraController`；`SerialCameraController` 已实现但尚未注入具体 `ICameraSerialPort` |
| 数据交换联调 | GXTC/GDCL 端点、编码、显式绑定/关闭和错误显示已完成；仍需补发送样例与接收状态 |
| Sapera 硬件验证 | 可选实时源已注册，默认不启动；按 [硬件验证](hardware-validation.md) 在采集机执行 smoke test |
| CUDA 收益验证 | 策略工厂和基准入口已接入；按 [硬件验证](hardware-validation.md) 完成输出对照与性能门槛后再开放 UI |

## 依赖关系

```
dss_app
├── dss_core
├── dss_processing
├── dss_acquisition_qt
├── dss_comm_qt     (编译时条件: DSS_BUILD_APP)
└── dss_network_qt  (编译时条件: DSS_BUILD_APP)
```
