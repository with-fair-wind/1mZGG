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
| `wrap(frame)` | 写入帧头帧尾 |

### 2. SerialProtocolCodec (`serial_protocol_codec.h`)

所有四路协议的编解码函数（header-only），核心编解码逻辑集中在此。

**解码函数:**
- `decodeDisplayFrame(frame)` → `ExposureDisplayData`
- `decodeExposureFrame(frame)` → `ExposureDisplayData`
- `decodeMasterControlFrame(frame)` → `MasterControlCommand`

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
class ISerialChannel : public IService {
    virtual void open(config) = 0;
    virtual void close() = 0;
    virtual auto recvFrameSize() const -> size_t = 0;
    virtual auto sendFrameSize() const -> size_t = 0;
};
```

### 4. SerialWorkerBase (`serial_worker_base.h`)

Qt `QSerialPort` 工作线程基类（pimpl 隐藏 Qt 依赖）：

- 内部线程持有 `QSerialPort` 实例
- `open()` — 配置并打开串口
- 收到完整帧后调用虚函数 `decodeFrame()`
- 发送时调用虚函数 `encodeFrame()`

### 5. 四路通道实现

| 类 | 旧版 | 接收处理 | 发送处理 |
|---|------|---------|---------|
| `DisplayChannel` | `CommDisplay` | 解码 → 发布 `Sync25HzEvent` | — |
| `ExposureChannel` | `CommExposure` | 解码 → 缓存 `latestData()` + 发布 `ExposureSyncEvent` | 曝光指令 |
| `MasterControlChannel` | `CommMasterControl` | 解码 → 发布 `MasterControlEvent` | 主控状态回复 |
| `ServoChannel` | `CommServo` | — | `setTrackResult()` → 编码修正帧 |

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
| 串口打开命令未接线 | 四路串口服务已注册到 `ServiceRegistry`，但默认 `isOpen() == false`；需要 UI/联调入口显式调用 `open()` |
| 错误处理 | 串口断连/重连机制待完善 |

## 依赖关系

```
dss_comm_qt
├── dss_core
├── Qt6::Core
└── Qt6::SerialPort
```
