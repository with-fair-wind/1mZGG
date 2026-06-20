# Acquisition 模块

> 命名空间: `Dss::Acquisition`
>
> 头文件: `include/dss/acquisition/`
>
> 源文件: `src/acquisition/`
>
> 目标: `dss_acquisition_qt`

## 模块职责

Acquisition 模块定义相机采集、回放帧源与控制接口。当前支持图像序列回放、实时/回放协调与可选 Sapera SDK 采集器；默认无硬件构建可驱动处理、显示和存储链路。

## 组件清单

### 1. IFrameSource（隐含于接口设计）

帧采集源接口，定义采集/回放源的生命周期和帧回调方法：

- `init()` — 初始化帧源
- `start()` — 开始采集
- `stop()` — 停止采集
- `setFrameCallback()` — 设置 `FramePacket` 输出回调
- `frameWidth()` / `frameHeight()` — 图像尺寸

### 2. ImageSequenceFrameSource (`image_sequence_frame_source.h`)

从旧版 `ImageReplayer` 思路迁入的图像序列回放源：

| 能力 | 状态 |
|------|------|
| 选择文件序列 | 已实现，UI 可传入 `QStringList` |
| 常规图像解码 | 已实现，使用 Qt `QImage` 读取 BMP/PNG/JPEG/TIFF 等 |
| legacy RAW 解码 | 已实现，复用 `decodeRawImageFile()` |
| 后台回放线程 | 已实现，按帧调用 `IFrameSource::FrameCallback` |
| UI 当前帧进度 | 已实现，`ViewModel` 根据 `DisplayRefreshEvent` 维护当前显示帧号 |
| 暂停续播/单帧前进 | 已实现暂停续播、单帧前进/后退、`seek(index)` 随机定位和进度快照 |

### 3. ICameraController (`i_camera_controller.h`)

相机命令接口，抽象相机参数控制：

```cpp
class ICameraController {
    virtual void setExposure(float ms) = 0;
    virtual void setGain(int value) = 0;
    virtual void setWindow(int x, int y, int w, int h) = 0;
    virtual void startGrab() = 0;
    virtual void stopGrab() = 0;
};
```

### 4. CameraControlProtocol (`camera_control_protocol.h`)

3 字节寄存器命令的编码函数，从旧版 `CommCamera` 迁移：

| 函数 | 用途 |
|------|------|
| `buildGrabCommand()` | 采集开始/停止命令 |
| `buildExposureCommand(ms)` | 曝光时间设置命令 |
| `buildGainCommand(value)` | 增益设置命令 |
| `buildWindowCommand(x, y, w, h)` | 采集窗口设置命令 |

每条命令格式: `[地址, 高字节, 低字节]`，通过串口发送到相机。

### 5. CommandOnlyCameraController

桩实现，仅生成命令字节但不连接串口。用于测试和开发阶段。

## 旧版对照

| 旧版 | 新版 | 状态 |
|------|------|------|
| `CommCamera.h/.cpp` (命令编码) | `camera_control_protocol.h` | **已迁移** |
| `CommCamera.h/.cpp` (串口通信) | `CommandOnlyCameraController` | 部分 (无串口连接) |
| `Grabber.h/.cpp` (Sapera SDK) | `SaperaFrameSource` + `ISaperaCaptureSession` | **可选适配器已实现** |
| `ImageReplayer.h/.cpp` (图像序列) | `ImageSequenceFrameSource` | **已迁移首版** |

## 当前缺口

| 缺口 | 严重程度 | 说明 |
|------|---------|------|
| 回放浏览控制 | 中 | 进度条、前进/后退、拖动定位和运行中 seek 已完成并有回归测试 |
| Sapera 采集器 | 中 | `SaperaFrameSource` 已通过 RAII 会话封装；无 SDK 构建已验证，硬件 smoke test 需在采集机执行 |
| 相机串口连接 | 中 | `SerialCameraController` 已适配窄串口接口，默认启动仍不打开串口 |

## 依赖关系

`IFrameSource` 接口依赖 `dss_processing` 的 `FramePacket`；`ImageSequenceFrameSource`
在 `dss_acquisition_qt` 中编译，`.cpp` 内部使用 Qt `QImage` 解码，头文件不直接暴露 Qt 图像头。
未来实际采集器将依赖 Sapera SDK（通过 `DSS_ENABLE_SAPERA` 选项控制）。
