# Acquisition 模块

> 命名空间: `Dss::Acquisition`
>
> 头文件: `include/dss/acquisition/`
>
> 源文件: 无独立编译目标（header-only）

## 模块职责

Acquisition 模块定义相机采集与控制的接口和协议。当前为 header-only，实际的 Sapera SDK 采集器尚未迁移。

## 组件清单

### 1. IFrameSource（隐含于接口设计）

帧采集源接口，定义相机的生命周期和帧获取方法：

- `init()` — 初始化相机
- `start()` — 开始采集
- `stop()` — 停止采集
- `width()` / `height()` — 图像尺寸

### 2. ICameraController (`i_camera_controller.h`)

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

### 3. CameraControlProtocol (`camera_control_protocol.h`)

3 字节寄存器命令的编码函数，从旧版 `CommCamera` 迁移：

| 函数 | 用途 |
|------|------|
| `buildGrabCommand()` | 采集开始/停止命令 |
| `buildExposureCommand(ms)` | 曝光时间设置命令 |
| `buildGainCommand(value)` | 增益设置命令 |
| `buildWindowCommand(x, y, w, h)` | 采集窗口设置命令 |

每条命令格式: `[地址, 高字节, 低字节]`，通过串口发送到相机。

### 4. CommandOnlyCameraController

桩实现，仅生成命令字节但不连接串口。用于测试和开发阶段。

## 旧版对照

| 旧版 | 新版 | 状态 |
|------|------|------|
| `CommCamera.h/.cpp` (命令编码) | `camera_control_protocol.h` | **已迁移** |
| `CommCamera.h/.cpp` (串口通信) | `CommandOnlyCameraController` | 部分 (无串口连接) |
| `Grabber.h/.cpp` (Sapera SDK) | `IFrameSource` 接口 | **未迁移** |

## 当前缺口

| 缺口 | 严重程度 | 说明 |
|------|---------|------|
| Sapera 采集器 | **高** | `Grabber` 类 (~500行) 未迁移，系统无法实际采集图像 |
| 模拟帧源 | 中 | 无测试用模拟帧源实现 |
| 相机串口连接 | 中 | `CommandOnlyCameraController` 未连接到实际串口通道 |

## 依赖关系

当前 header-only，仅依赖 `dss_core` 类型。
未来实际采集器将依赖 Sapera SDK（通过 `DSS_ENABLE_SAPERA` 选项控制）。
