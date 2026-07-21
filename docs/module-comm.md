# Comm 模块 (`dss_comm_qt`)

> 命名空间: `Dss::Comm`
>
> 头文件: `include/dss/comm/`
>
> 源文件: `src/comm/`
>
> 依赖: `dss_core`, `Qt6::Core`, `Qt6::SerialPort`

## 模块职责

Comm 模块封装四路串口通信通道，负责与天文设备的低层协议交互。每路串口有独立的帧格式（收/发字节数不同），通过统一的 `SerialWorkerBase` 基类和 `std::jthread` 管理 Qt 串口 I/O。

## 协议帧格式

所有串口帧使用相同的封帧方式：
- 帧头: `0x7E`
- 帧尾: `0xE7`
- 数据: 中间字节

| 通道 | 接收帧长 | 发送帧长 | 用途 |
|------|---------|---------|------|
| Display | 20 字节 | 9 字节 | 显示同步 (25Hz) |
| Exposure | 23 字节 | 8 字节 | 曝光参数同步 |
| MasterControl | 30 字节 | 28 字节 | 主控指令 |
| Servo | 20 字节 | 14 字节 | 伺服修正 |

## 组件清单

### 1. FrameCodec (`frame_codec.h`)

帧编解码基础：

| 方法 | 说明 |
|------|------|
| `validate(frame, expectedSize)` | 验证帧头/尾和长度 |
| `validateDetailed(frame, expectedSize)` | 返回帧长、头尾字节和失败原因，供日志/诊断使用 |
| `failureMessage(reason)` | 将失败原因转换为稳定诊断文本 |
| `wrap(frame)` | 写入帧头帧尾 |

### 2. SerialProtocolCodec (`serial_protocol_codec.h`)

所有四路协议的编解码函数（header-only），核心编解码逻辑集中在此。

**解码函数:**
- `decodeDisplayFrame(frame)` → `ExposureDisplayData`
- `decodeExposureFrame(frame)` → `ExposureDisplayData`
- `decodeMasterControlFrame(frame)` → `MasterControlCommand`
- `decode*FrameDetailed(frame)` → 保留 `SerialDecodeError` 的字段名、偏移和原始值，用于接收诊断

**编码函数:**
- `encodeExposureCommand(command, frame)` → 曝光指令帧
- `encodeServoCorrection(correction, frame)` → 伺服修正帧
- `encodeMasterControlStatus(status, frame)` → 主控状态帧

**数据编码细节:**
- 时间: BCD 编码 (`decodeBcd` / `encodeBcd`)
- 角度: 29 位定点数 (0~360°，精度约 0.00000067°)
- 距离/速度: 有符号幅值编码 (16/24 位)
- 字节序: 小端 (Little-Endian)

### 3. ISerialChannel (`i_serial_channel.h`)

串口通道抽象接口：

```cpp
class ISerialChannel {
    virtual void open(config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual auto recvFrameSize() const -> size_t = 0;
    virtual auto sendFrameSize() const -> size_t = 0;
};
```

### 4. 串口命令接口 (`serial_command_interfaces.h`)

命令发送入口按职责拆成窄接口，避免 UI 或 ViewModel 依赖具体通道类：

| 接口 | 注册名 | DTO | 用途 |
|------|--------|-----|------|
| `IExposureCommandPort` | `exposure` | `ExposureCommand` | 曝光触发模式、帧频编码、曝光延迟 |
| `IServoCorrectionPort` | `servo` | `ServoCorrection` | 伺服距离/速度修正 |
| `IMasterControlStatusPort` | `master_control` | `MasterControlStatus` | 主控状态回包 |

这些接口只缓存 DTO 并请求发送，真实串口仍必须经 `ISerialChannel::open()` 显式打开。

### 5. SerialWorkerBase (`serial_worker_base.h`)

Qt `QSerialPort` 工作线程基类（公开头只保留前置声明）：

- 内部线程持有 `QSerialPort` 实例
- `open()` — 配置并打开串口
- 收到完整帧后调用虚函数 `decodeFrame()`
- 发送时调用虚函数 `encodeFrame()`
- 接收帧头尾/长度校验失败时发布 `SerialFrameErrorEvent`，由 UI 日志页展示
- 接收帧通过固定帧校验但字段解码失败时发布 `SerialDecodeErrorEvent`

### 6. 四路通道实现

| 类 | 旧版 | 接收处理 | 发送处理 |
|---|------|---------|---------|
| `DisplayChannel` | `CommDisplay` | 解码 → 发布 `Sync25HzEvent` | — |
| `ExposureChannel` | `CommExposure` | 解码 → 缓存 `latestData()` + 发布 `ExposureSyncEvent` | `IExposureCommandPort` → 曝光指令 |
| `MasterControlChannel` | `CommMasterControl` | 解码 → 发布 `MasterControlEvent` | `IMasterControlStatusPort` → 主控状态回复 |
| `ServoChannel` | `CommServo` | — | `setTrackResult()`/`IServoCorrectionPort` → 编码修正帧 |

## 帧数据布局 (接收)

### Display 帧 (20 字节)
```
[0]     帧头 0x7E
[1]     年 (BCD, +2000)
[2]     月 (BCD)
[3]     日 (BCD)
[4]     时 (BCD)
[5]     分 (BCD)
[6]     秒 (BCD)
[7-8]   毫秒 (U16LE, ÷10)
[9-12]  方位角 (U32LE, 角度码)
[13-16] 俯仰角 (U32LE, 角度码)
[17-18] (保留)
[19]    帧尾 0xE7
```

### MasterControl 帧 (30 字节)
```
[0]     帧头 0x7E
[1-2]   (保留)
[3-5]   曝光时间 (U24LE, ÷100 → ms)
[6]     (保留)
[7]     mode1
[8]     mode2
[9]     跟踪开关 (0xFF=开)
[10]    存储开关 (0xFF=开)
[11-13] 目标ID (U24LE)
[14-16] 任务ID (U24LE)
[17-19] 开始时间 (时/分/秒)
[20-22] 结束时间 (时/分/秒)
[23-28] (保留)
[29]    帧尾 0xE7
```

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 接收侧运行诊断仍需细化 | 帧长/头尾校验失败已发布 `SerialFrameErrorEvent`；显示/曝光 BCD 时间和主控时间窗口字段解码失败已发布 `SerialDecodeErrorEvent`，并进入 UI 日志页 Warning 级别缓存；后续补统计聚合和更多协议字段约束 |
| 错误处理 | 串口断连/重连机制待完善 |

## 依赖关系

```
dss_comm_qt
├── dss_core
├── Qt6::Core
└── Qt6::SerialPort
```
## 深入架构与调用链

### 模块边界与依赖

Comm 把四路遗留定长串口协议封装成统一通道和类型化命令端口。它负责串口打开、工作循环、帧校验、编解码和事件发布；不负责 UI 表单、跟踪算法或网络协议。

```mermaid
flowchart LR
    COMM["dss_comm_qt"] --> CORE["dss_core"]
    COMM --> QT["Qt6 Core/SerialPort"]
    APP["ApplicationContext"] --> COMM
    UI["SerialPortViewModel"] -->|"open/close/send"| COMM
    COMM -->|"同步与错误事件"| BUS["MessageBus"]
```

### 关键类关系

```mermaid
classDiagram
    class ISerialChannel {
        <<interface>>
        +open(SerialConfig) expected
        +close()
        +isOpen() bool
        +status() Status
        +recvFrameSize() size_t
        +sendFrameSize() size_t
    }
    class SerialWorkerBase {
        -jthread workerThread
        +open(config)
        +close()
        #requestSend()
        #decodeFrame(data)*
        #encodeFrame(buffer)*
    }
    class DisplayChannel
    class ExposureChannel {
        +latestData()
        +sendExposureCommand(command)
    }
    class MasterControlChannel {
        +sendMasterControlStatus(status)
    }
    class ServoChannel {
        +setTrackResult(target)
        +sendServoCorrection(correction)
    }
    class FrameCodec {
        +validateDetailed(frame,size)
        +wrap(frame)
    }
    class IExposureCommandPort
    class IMasterControlStatusPort
    class IServoCorrectionPort

    ISerialChannel <|.. SerialWorkerBase
    SerialWorkerBase <|-- DisplayChannel
    SerialWorkerBase <|-- ExposureChannel
    SerialWorkerBase <|-- MasterControlChannel
    SerialWorkerBase <|-- ServoChannel
    ExposureChannel ..|> IExposureCommandPort
    MasterControlChannel ..|> IMasterControlStatusPort
    ServoChannel ..|> IServoCorrectionPort
    SerialWorkerBase ..> FrameCodec
```

### 打开与工作线程

```mermaid
sequenceDiagram
    participant UI as SerialPortViewModel
    participant Base as SerialWorkerBase
    participant Worker as std::jthread
    participant Port as QSerialPort

    UI->>Base: open(SerialConfig)
    Base->>Worker: 启动 workerLoop + init promise
    Worker->>Port: 在线程内构造并设置端口/波特率/8N1/无流控
    Worker->>Port: open(ReadWrite)
    alt 打开失败
        Worker-->>Base: unexpected(errorString)
        Base-->>UI: unexpected(errorString)
    else 成功
        Worker->>Base: status=Ok + promise success
        Base-->>UI: success
    end
```

`QSerialPort` 在 `std::jthread` 内构造、打开、阻塞读写、关闭并销毁，完整生命周期保持同一线程亲和性。`open()` 仅通过 `std::promise/std::future` 同步取得初始化结果，不引入 `QThread`。

### 接收调用栈

```mermaid
flowchart TD
    WAIT["workerLoop: waitForReadyRead(20ms)"] --> READ["onDataReceived: readAll 追加到累积缓冲"]
    READ --> DRAIN["drainBufferedFrames 反复取完整帧"]
    DRAIN --> SCAN{"首字节 == 0x7E?"}
    SCAN -->|"否"| SYNC["扫描下一个 0x7E 丢弃失步前缀"]
    SCAN -->|"是"| TAKE["取 recvFrameSize 字节一帧"]
    TAKE --> VALID["FrameCodec::validateDetailed"]
    VALID -->|"头/尾/长度正确"| DECODE["派生类 decodeFrame + 消费整帧"]
    VALID -->|"失败"| DROP["丢首字节滑窗 + SerialFrameErrorEvent"]
    DECODE -->|"字段合法"| DOMAIN["发布领域事件/更新缓存"]
    DECODE -->|"BCD/范围等错误"| DE["SerialDecodeErrorEvent"]
    DOMAIN --> FPS["recvCount++"]
```

基础层做定长帧和首尾字节校验；字段范围由 `serial_protocol_codec.h` 详细解码器检查。接收采用**流式重同步状态机**(A3):`onDataReceived` 用 `readAll` 把字节追加到 `m_rxAccumulator`,再 `drainBufferedFrames` 反复取完整帧——首字节非帧头时向前扫描到下一个 `0x7E` 丢弃失步前缀(静默对齐),帧头对齐但尾校验失败时丢首字节逐步滑窗重同步;跨 `waitForReadyRead` 边界累积半帧,`close()` 时清空缓冲防重连拼接。发送侧 `sendFrameInternal` 循环写完整帧(部分写续写),`write<=0` 发布 `SerialFrameErrorEvent` 且不递增计数,发送在 `m_sendMutex` 锁外执行避免 `emit` 与 `requestSend` 重入死锁(B4)。

### 发送调用栈

```mermaid
sequenceDiagram
    participant UI as 调用方
    participant Channel as 具体 Channel
    participant Base as SerialWorkerBase
    participant Worker as 工作线程
    participant Codec as 协议编码
    participant Port as QSerialPort

    UI->>Channel: send...Command(value)
    Channel->>Channel: mutex 下覆盖 pending value
    Channel->>Base: requestSend()
    Base->>Base: sendRequested=true
    Worker->>Base: 发现发送请求并清零
    Base->>Channel: encodeFrame(buffer)
    Channel->>Codec: encode...
    Base->>Base: FrameCodec::wrap(header/tail)
    Base->>Port: write(buffer)
```

发送请求是布尔标志，不是命令队列；工作线程取走之前的多次调用会合并为“发送最新 pending 值”。这适合状态/修正量更新，不适合要求逐条可靠送达的事务命令。

### 四路协议行为

| 通道 | 接收成功 | 发送载荷 | 运行时接口 |
|---|---|---|---|
| Display | 解码指向时间数据后发布 `Sync25HzEvent` | 当前无有效载荷 | `ISerialChannel` |
| Exposure | 缓存 `ExposureDisplayData`，发布 `ExposureSyncEvent` | `ExposureCommand` | `IExposureCommandPort` |
| MasterControl | 转换并发布 `MasterControlEvent` | `MasterControlStatus` | `IMasterControlStatusPort` |
| Servo | 当前接收解码为空 | `ServoCorrection` | `IServoCorrectionPort` |

`ServoChannel::setTrackResult()` 可把 `TargetInfo` 的 AE 位置/速度换算成角秒修正并请求发送，但当前 App 没有自动订阅 `TrackResultEvent` 调用它；通信页支持显式发送修正命令。

### 协议层次

```mermaid
flowchart LR
    BYTES["原始字节"] --> FRAME["FrameCodec<br/>长度/帧头/帧尾"]
    FRAME --> LAYOUT["layoutFor(SerialProtocol)<br/>固定收发长度"]
    LAYOUT --> DETAIL["detail 编解码<br/>小端/BCD/角度/符号幅值"]
    DETAIL --> DTO["ExposureCommand / MasterControlCommand / ServoCorrection"]
    DTO --> EVENT["Core 事件或发送缓存"]
```

所有多字节字段、缩放系数和 BCD 范围应集中在 codec；Channel 只做状态缓存和事件映射。新增字段不要在 UI 或 worker 里直接按偏移读写。

### 生命周期、线程与错误

| 状态/数据 | 保护 |
|---|---|
| status、收发计数/FPS | atomic |
| `sendRequested` | send mutex |
| 各通道 pending 命令与 latest data | 各自 mutex |
| QSerialPort | 只在所属 `std::jthread` 内构造、访问和销毁 |
| 总线事件 | 在串口 worker 线程同步发布 |

`close()` 请求停止并 join；工作线程退出前关闭/释放串口，调用线程只重置状态。帧结构错误和字段解码错误分别发布不同事件，`ErrorDiagnostics` 会把 Display/Exposure 错误标记为通信失败，`RuntimeDiagnostics` 统一累计串口错误数。

### 配置、扩展与测试

端口名和波特率来自 `Config::comm()`，UI 可编辑并保存；应用修改配置时会先关闭已打开通道，避免在运行中替换端口参数。新增协议应先增加 `SerialProtocol` layout 和 codec 测试，再实现薄 Channel，最后注册接口和 UI。

重点测试：`test_frame_codec.cpp`、`test_serial_protocol_codec.cpp`、`test_serial_port_view_model.cpp`、`test_application_context_services.cpp`。流式重同步(连续帧/垃圾字节/尾错/跨读取半帧/close 清缓存)已有单元测试覆盖(`test_application_context_services` 的 `SerialResync` 套);真实串口仍需硬件验证断线、持续高频发送和关闭竞态。

推荐源码顺序：`i_serial_channel.h` → `frame_codec.h` → `serial_protocol_codec.h` / detail → `serial_command_interfaces.h` → `serial_worker_base.*` → 四个 Channel → App 注册 → `SerialPortViewModel` 和通信面板。
