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
| `wireLogger()` | 初始化日志系统，连接到事件总线 |
| `loadConfig(path)` | 加载 JSON 配置文件，返回 `std::expected` |
| `registerCommunicationServices()` | 注册所有串口/网络/存储/相机服务 |
| `startServices()` | 按序启动所有已注册服务 |
| `stopServices()` | 按逆序停止所有服务 |

### 通信服务注册 (`communication_services.cpp`)

`registerCommunicationServices()` 内部创建并注册以下服务实例。注册后默认不打开串口、不绑定 UDP、不触碰真实硬件；回放源、处理器和存储 worker 由 UI 命令显式启动。

| 服务 | 模块 | 注册名 |
|------|------|--------|
| `DisplayChannel` / `ISerialChannel` | dss_comm_qt | `display` |
| `ExposureChannel` / `ISerialChannel` | dss_comm_qt | `exposure` |
| `MasterControlChannel` / `ISerialChannel` | dss_comm_qt | `master_control` |
| `ServoChannel` / `ISerialChannel` | dss_comm_qt | `servo` |
| `ImageSender` / `INetworkChannel` | dss_network_qt | `image_sender` |
| `Heartbeat` / `INetworkChannel` | dss_network_qt | `heartbeat` |
| `ErrorDiagnostics` / `INetworkChannel` | dss_network_qt | `error_diagnostics` |
| `DataExchange` | dss_network_qt | `data_exchange` |
| `TrackResultDataExchangeBridge` | dss_app | `track_result_data_exchange_bridge` |
| `AtmosReceiver` / `INetworkChannel` | dss_network_qt | `atmos_receiver` |
| `ImageProcessor` | dss_processing | `image_processor` |
| `ImageSequenceFrameSource` / `IFrameSource` | dss_acquisition | `replay_source` |
| `LocalImageStorageBackend` / `IStorageBackend` | dss_storage | `image_storage` |
| `TrackDataStorageBackend` / `IStorageBackend` | dss_storage | `track_data_storage` |
| `CommandOnlyCameraController` | dss_acquisition | `camera` |

## 启动流程 (`main.cpp`)

```
1. QApplication 初始化
2. ApplicationContext 构造
3. wireLogger()           → 日志系统就绪
4. loadConfig()           → JSON 配置加载
5. InitDialog 显示        → 初始化进度展示
6. registerCommunicationServices()  → 服务注册
7. ViewModel 构造         → 订阅事件
8. MainWindow 构造/显示   → UI 就绪
9. QApplication::exec()  → 事件循环
```

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 显式硬件启动入口未接线 | 串口/UDP 服务已注册，但默认不打开硬件；通信页已统一编辑网络端点，图像发送/心跳/诊断/大气接收已通过 `INetworkChannel` 统一控制，数据交换已有专用显式 open/close，显示/曝光/主控/伺服串口已通过 `ISerialChannel` 完成参数编辑和显式 open/close；Sapera 采集和更细的协议联调命令仍需 UI/联调入口触发 |
| 数据交换发送配置待完善 | 跟踪结果已能经桥接器转换为 GXTC/GDCL DTO；`ViewModel` 可编辑网络端点并显式绑定/关闭配置中的 UDP，网络错误事件会显示到状态栏；日志面板分级展示待补 |
| 回放策略仍是首版 | `replay_source` 可按序播放文件，暂停续播/进度恢复待补 |
| `CudaDeviceManager` 未初始化 | GPU 后端未接入 (有 TODO 注释) |
| Sapera `IFrameSource` 未实现 | 真实相机采集器尚无实现，当前主入口是回放源 |

## 依赖关系

```
dss_app
├── dss_core
├── dss_processing
├── dss_acquisition_qt
├── dss_comm_qt     (编译时条件: DSS_BUILD_APP)
└── dss_network_qt  (编译时条件: DSS_BUILD_APP)
```
