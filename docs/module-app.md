# App 模块 (`dss_app`)

> 命名空间: `Dss::App`
>
> 头文件: `include/dss/app/`
>
> 源文件: `src/app/`
>
> 依赖: `dss_core`, `dss_comm_qt`, `dss_network_qt`

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

`registerCommunicationServices()` 内部创建并注册以下服务实例：

| 服务 | 模块 | 注册名 |
|------|------|--------|
| `DisplayChannel` | dss_comm_qt | 显示同步串口 |
| `ExposureChannel` | dss_comm_qt | 曝光同步串口 |
| `MasterControlChannel` | dss_comm_qt | 主控串口 |
| `ServoChannel` | dss_comm_qt | 伺服串口 |
| `ImageSender` | dss_network_qt | 图像 UDP 发送 |
| `Heartbeat` | dss_network_qt | 心跳 UDP |
| `ErrorDiagnostics` | dss_network_qt | 诊断 UDP |
| `DataExchange` | dss_network_qt | 数据交换 UDP |
| `AtmosReceiver` | dss_network_qt | 大气数据接收 |
| `LocalImageStorageBackend` | dss_storage | 图像本地存储 |
| `TrackDataStorageBackend` | dss_storage | 轨迹数据存储 |
| `CommandOnlyCameraController` | dss_acquisition | 相机控制 (命令) |

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
| `startServices()` 未调用 | `main.cpp` 中注册了服务但未启动 |
| `ImageProcessor` 未注册 | 处理管线尚未连接到应用上下文 |
| `CudaDeviceManager` 未初始化 | GPU 后端未接入 (有 TODO 注释) |
| `IFrameSource` 未注册 | 相机采集器尚无实现 |

## 依赖关系

```
dss_app
├── dss_core
├── dss_comm_qt     (编译时条件: DSS_BUILD_APP)
└── dss_network_qt  (编译时条件: DSS_BUILD_APP)
```
