# Network 模块 (`dss_network_qt`)

> 命名空间: `Dss::Network`
>
> 头文件: `include/dss/network/`
>
> 源文件: `src/network/`
>
> 依赖: `dss_core`, `Qt6::Core`, `Qt6::Network`

## 模块职责

Network 模块封装所有 UDP 网络通信，负责图像传输、心跳保活、诊断信息上报、大气数据接收、数据交换等功能。

## 组件清单

### 1. INetworkChannel (`i_network_channel.h`)

UDP 单端点服务抽象接口；图像发送、心跳、诊断和大气接收均实现该接口，并在应用注册时同时按具体类型和 `INetworkChannel` 注册：

```cpp
class INetworkChannel {
    virtual auto open(config) -> std::expected<void, std::string> = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual auto status() const -> Status = 0;
};
```

### 2. UdpChannel (`udp_channel.h`)

Qt `QUdpSocket` 封装。每个通道使用独立 `std::jthread`，socket 在该线程内完成构造、绑定、收发、关闭和销毁：

| 方法 | 说明 |
|------|------|
| `bind(config)` | 绑定本地端点；本地端口为 0 时由系统分配临时端口 |
| `send(data)` | 发送到预设目标 |
| `sendTo(data, host, port)` | 发送到指定目标 |
| `localPort()` | 获取实际绑定端口，支持测试和端口 0 场景 |
| `setReceiveCallback(fn)` | 设置接收回调 |

### 3. ImageSender (`image_sender.h`)

分片 UDP 图像传输（从旧版 `NetImageSender` 迁移）。

**设计:**
- 最大单帧载荷: 60KB
- 大图自动分片传输
- 独立工作线程发送

| 方法 | 说明 |
|------|------|
| `buildPackets(imageData)` | 将图像切分为 UDP 分片 |
| `sendImage(imageData)` | 异步发送图像 |

### 4. Heartbeat (`heartbeat.h`)

心跳保活服务（从旧版 `NetApp` 心跳部分迁移）。

| 方法 | 说明 |
|------|------|
| `buildFrame()` | 构建心跳帧 |
| `buildCloseGuardFrame()` | 构建关闭保护帧 |

### 5. ErrorDiagnostics (`error_diagnostics.h`)

JSON 格式诊断信息上报（从旧版 `NetErrorDiagnose` 迁移）。

| 方法 | 说明 |
|------|------|
| `setStatus(key, value)` | 设置诊断状态项 |
| `buildDiagnosticStatusJson()` | 生成 JSON 诊断报文 |

### 6. DataExchange (`data_exchange.h`)

GXTC/GDCL 协议数据交换（从旧版 `NetExchange` 迁移）。

| 方法 | 说明 |
|------|------|
| `sendGxtc(data)` | 发送 GXTC 协议数据包，失败时返回错误并发布 `NetworkTransmissionErrorEvent` |
| `sendGdcl(data)` | 发送 GDCL 协议数据包，失败时返回错误并发布 `NetworkTransmissionErrorEvent` |
| `makeGxtcMetadata(packet, options)` | 从 `ResultPacket` 构造 GXTC 头部 DTO |
| `makeGxtcTarget(packet, options)` | 从 `ResultPacket` 构造 GXTC 目标 DTO |
| `makeGdclMeasurement(packet, options)` | 从 `ResultPacket` 构造 GDCL 测量 DTO |
| `makeJms1970Centiseconds(timestamp)` | 将通用时间戳换算为旧协议 JMS1970 百分之一秒 |

### 7. AtmosReceiver (`atmos_receiver.h`)

大气遥测数据 UDP 接收（从旧版 `NetAtmos` 迁移）。

| 方法 | 说明 |
|------|------|
| `decodeAtmosPacket(data)` | 解码大气数据包 (温度/气压/湿度) |

接收后发布 `AtmosphereDataEvent` 到消息总线。

### 8. 协议头文件 (header-only)

| 文件 | 用途 |
|------|------|
| `atmos_protocol.h` | 大气数据包编解码 |
| `data_exchange_protocol.h` | GXTC/GDCL 数据包编解码，以及 `ResultPacket` 到协议 DTO 的纯映射 |
| `diagnostic_protocol.h` | 诊断 JSON 报文构建 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 联调样例与接收状态 | 服务注册、统一端点编辑、显式开关、错误日志落盘/搜索/导出均已完成；仍需补产品化发送样例和接收状态展示 |
| `NetApp` 接收侧逻辑 | 仅迁移当前业务需要的心跳和网络服务，旧版其余接收处理未逐行复制 |
| `ENetServer` | 旧版可靠 UDP 未迁移；当前业务链路不依赖 ENet |

## 依赖关系

```
dss_network_qt
├── dss_core
├── Qt6::Core
└── Qt6::Network
```
## 深入架构与调用链

### 模块边界与依赖

Network 负责 UDP socket、协议 DTO 映射、分片和周期发送。它不决定何时产生跟踪结果，也不直接读取 Widget；App 桥接器把核心事件映射到 `DataExchange`。

```mermaid
flowchart LR
    NET["dss_network_qt"] --> CORE["dss_core"]
    NET --> QT["Qt6 Core/Network"]
    APP["ApplicationContext/Bridge"] --> NET
    UI["NetworkViewModel/DataExchangeViewModel"] -->|"配置与显式开关"| NET
    NET -->|"错误/气象/发送事件"| BUS["MessageBus"]
```

### 关键类关系

```mermaid
classDiagram
    class INetworkChannel {
        <<interface>>
        +open(UdpEndpointConfig) expected
        +close()
        +isOpen() bool
        +status() Status
    }
    class UdpChannel {
        -jthread ioWorker
        -deque~SendRequest~ sendQueue
        +bind(config)
        +send(data)
        +sendTo(data,host,port)
        +localPort()
        +setReceiveCallback(cb)
    }
    class ImageSender {
        -UdpChannel channel
        -jthread worker
        +sendImage(data,width,height)
        +buildPackets(data,width,height)
    }
    class Heartbeat {
        -UdpChannel channel
        -jthread worker
        +sendCloseGuard()
    }
    class ErrorDiagnostics {
        -DiagnosticStatus status
        -UdpChannel channel
        -jthread worker
    }
    class AtmosReceiver {
        -UdpChannel channel
        +onData(data,sender,port)
    }
    class DataExchange {
        -UdpChannel gxtcChannel
        -UdpChannel gdclChannel
        +sendGxtc(...)
        +sendGdcl(...)
    }

    UdpChannel <-- ImageSender
    UdpChannel <-- Heartbeat
    UdpChannel <-- ErrorDiagnostics
    UdpChannel <-- AtmosReceiver
    UdpChannel <-- DataExchange
    INetworkChannel <|.. ImageSender
    INetworkChannel <|.. Heartbeat
    INetworkChannel <|.. ErrorDiagnostics
    INetworkChannel <|.. AtmosReceiver
```

`DataExchange` 管理两条端点，故没有实现单端点的 `INetworkChannel`；它由专用 ViewModel 同时打开 GXTC/GDCL。

### UdpChannel 基础调用栈

```mermaid
sequenceDiagram
    participant Caller as 服务/ViewModel
    participant Channel as UdpChannel
    participant Worker as std::jthread
    participant Socket as QUdpSocket
    participant Callback as ReceiveCallback

    Caller->>Channel: bind(config)
    Channel->>Worker: 启动 I/O 循环 + init promise
    Worker->>Socket: 构造并 bind(localIp,localPort)
    Worker-->>Channel: 绑定结果
    Worker->>Socket: waitForReadyRead()
    loop hasPendingDatagrams
        Worker->>Socket: receiveDatagram()
        Worker->>Callback: callback(span,sender,port)
    end
    Caller->>Channel: send(data)
    Channel->>Worker: 入队 SendRequest + future
    Worker->>Socket: writeDatagram(remoteIp,remotePort)
    Worker-->>Caller: 实际发送字节数
```

接收 callback 的 `span` 只在本次调用内有效，订阅者若异步保存必须复制。callback 在 mutex 下取快照后执行，避免业务处理时长期持锁。

### 图像发送分片链

```mermaid
flowchart TD
    CALL["ImageSender::sendImage"] --> LATEST["覆盖 pendingImage/宽高"]
    LATEST --> WAKE["notify worker"]
    WAKE --> ENCODE["10 字节图像头 + 8位像素"]
    ENCODE --> SPLIT["按 MaxUdpPayload 分片"]
    SPLIT --> HEADER["每片添加序号/总片数与 padding"]
    HEADER --> SEND["UdpChannel::send 每个分片"]
    SEND --> EVENT["ImageSendCompletedEvent{frameSeq}"]
```

ImageSender 只保留最新 pending 图像(单槽)，不是无界队列；处理速度跟不上时旧待发图会被覆盖。`ImageProcessor` 发布 `ImageReadyForSendEvent`：无 RAW 时直接携带共享 8 位缓冲，有 RAW 时携带延迟图像工厂。`ApplicationContext` 订阅回调仅在发送服务已打开时调 `submitForSend()`(image + imageFactory 入单槽),**不执行工厂**;`imageFactory`(整图拉伸)在 ImageSender 工作线程按需调用,不占 ImageProcessor 处理线程(B10)。全部分片成功提交后才发布对应帧序号的 `ImageSendCompletedEvent`。

### 跟踪结果到 GXTC/GDCL

```mermaid
sequenceDiagram
    participant Bridge as TrackResultDataExchangeBridge
    participant Map as data_exchange_protocol
    participant Exchange as DataExchange
    participant GXTC as UdpChannel GXTC
    participant GDCL as UdpChannel GDCL
    participant Bus as MessageBus

    Bridge->>Map: makeGxtcMetadata/Targets 或 makeGdclMeasurement
    Map-->>Bridge: 协议 DTO
    Bridge->>Exchange: sendGxtc/sendGdcl
    Exchange->>Map: build...Packet
    Exchange->>GXTC: send(packet)
    Exchange->>GDCL: send(packet)
    alt writeDatagram 失败/通道未开
        Exchange->>Bus: NetworkTransmissionErrorEvent
    end
```

`DataExchange::open()` 先绑定 GXTC，再绑定 GDCL。当前第二条绑定失败时没有自动关闭已成功的第一条，调用方应执行 `close()`；这是后续可补的原子打开/回滚点。

### 心跳、诊断与气象

| 服务 | 数据方向 | 执行方式 | 事件 |
|---|---|---|---|
| `Heartbeat` | 周期发送固定 10 字节帧 | `std::jthread` | 无；可显式发送 close-guard |
| `ErrorDiagnostics` | 周期发送诊断 JSON | `std::jthread`，状态用 mutex | 订阅串口/存储/网络错误 |
| `AtmosReceiver` | 接收并解码气象报文 | `UdpChannel` I/O worker | 成功发布 `AtmosphereDataEvent` |
| `ImageSender` | 异步发送图像分片 | 图像 worker + `UdpChannel` I/O worker | 成功后发布 `ImageSendCompletedEvent` |
| `DataExchange` | 同步提交结果报文 | 调用者入队，`UdpChannel` I/O worker 写 socket | 失败发布网络错误 |

```mermaid
flowchart LR
    SFE["SerialFrame/DecodeError"] --> ED["ErrorDiagnostics"]
    SWE["StorageWriteError"] --> ED
    NTE["NetworkTransmissionError"] --> ED
    ED --> JSON["DiagnosticStatus JSON"]
    JSON --> UDP["诊断 UDP"]
    DATAGRAM["气象 UDP"] --> DECODE["decodeAtmosPacket"]
    DECODE --> AE["AtmosphereDataEvent"]
```

### 生命周期与线程边界

`NetworkViewModel` 和 `DataExchangeViewModel` 通过 Registry 获取服务，应用端点配置后显式 `open()`，重配前先关闭。Heartbeat、ErrorDiagnostics、ImageSender 保留各自业务工作线程；所有服务持有的 `UdpChannel` 均有独立 I/O worker。

`QUdpSocket` 只在所属 I/O worker 内访问。`bind()` 用 promise 返回初始化结果；跨线程 `send()` 复制报文字节后入队，并用 future 返回 `writeDatagram()` 结果。接收回调也在 I/O worker 执行，订阅者触碰 Qt UI 时仍必须切回对象所属线程。

### 错误与可观测性

- bind 失败通过 `expected<string>` 返回，UI 显示错误。
- `UdpChannel::send()` 在未绑定或写失败时返回 -1；`DataExchange` 将其升级为 `NetworkTransmissionErrorEvent`。
- ImageSender 逐分片检查发送结果并发布 `NetworkTransmissionErrorEvent`；Heartbeat、ErrorDiagnostics 仍未把周期发送失败升级为事件。
- Atmos 解码失败直接丢弃，不发布错误事件；若现场需要区分链路静默与坏包，应增加计数或专用事件。
- UDP 本身无到达/顺序保证，图像接收端必须按分片头重组并处理丢片。

### 配置、扩展与测试

端点配置来自 `Config::comm()`，普通网络服务与 GXTC/GDCL 双端点分别由两个 ViewModel 管理。新增 UDP 服务时优先复用 `UdpChannel`，明确是同步发送、周期业务 worker 还是接收回调，并定义失败事件和关闭回滚。

重点测试：`test_network_protocols.cpp`、`test_data_exchange_protocol.cpp`、`test_data_exchange.cpp`、`test_image_sender.cpp`、`test_heartbeat.cpp`、`test_error_diagnostics.cpp`、`test_network_view_model.cpp`、`test_data_exchange_view_model.cpp`。

推荐源码顺序：`i_network_channel.h` → `udp_channel.*` → 各 protocol 头文件 → `data_exchange.*` → App 结果桥 → `image_sender.*` → `heartbeat.*` → `error_diagnostics.*` → `atmos_receiver.*` → 两个网络 ViewModel。
