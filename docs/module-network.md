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

UDP 端点抽象接口：

```cpp
class INetworkChannel : public IService {
    virtual void open(config) = 0;
    virtual void close() = 0;
    virtual auto status() const -> Status = 0;
};
```

### 2. UdpChannel (`udp_channel.h`)

Qt `QUdpSocket` 封装（pimpl 隐藏 Qt 依赖）：

| 方法 | 说明 |
|------|------|
| `bind(port)` | 绑定本地端口 |
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
| `sendGxtc(data)` | 发送 GXTC 协议数据包 |
| `sendGdcl(data)` | 发送 GDCL 协议数据包 |

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
| `data_exchange_protocol.h` | GXTC/GDCL 数据包编解码 |
| `diagnostic_protocol.h` | 诊断 JSON 报文构建 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 网络服务未启动 | `startServices()` 未调用 |
| `NetApp` 接收侧逻辑 | 旧版 `NetApp` 的接收处理未完全复制 |
| `ENetServer` | 旧版的可靠 UDP (ENet) 未迁移 |

## 依赖关系

```
dss_network_qt
├── dss_core
├── Qt6::Core
└── Qt6::Network
```
