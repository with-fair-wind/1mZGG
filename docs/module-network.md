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

Qt `QUdpSocket` 封装（pimpl 隐藏 Qt 依赖）：

| 方法 | 说明 |
|------|------|
| `bind(config)` | 绑定本地端点；本地端口为 0 时由系统分配临时端口 |
| `send(data)` | 发送到预设目标 |
| `sendTo(data, host, port)` | 发送到指定目标 |
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
