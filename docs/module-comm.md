# Comm 模块 (`dss_comm_qt`)

> 命名空间: `Dss::Comm`
>
> 头文件: `include/dss/comm/`
>
> 源文件: `src/comm/`
>
> 依赖: `dss_core`, `Qt6::Core`, `Qt6::SerialPort`

## 模块职责

Comm 模块封装四路串口通信通道，负责与天文设备的低层协议交互。每路串口有独立的帧格式（收/发字节数不同），通过统一的 `SerialWorkerBase` 基类管理 Qt 串口线程。

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

Qt `QSerialPort` 工作线程基类（pimpl 隐藏 Qt 依赖）：

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
