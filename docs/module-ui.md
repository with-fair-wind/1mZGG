# UI 模块 (`dss_ui_qt`)

> 命名空间: `Dss::Ui`
>
> 头文件: `include/dss/ui/`
>
> 源文件: `src/ui/`
>
> 依赖: `dss_core`, `dss_network_qt`, `dss_processing`, `dss_tracking`, `Qt6::Core/Gui/Widgets/Charts`

## 模块职责

UI 模块实现系统的图形用户界面，采用 MVVM 模式：`ViewModel` 持有 UI 状态并订阅事件总线，`MainWindow` 及各页面组件绑定到 ViewModel 的属性和信号。

## 架构模式

```
事件总线 (MessageBus)
    │
    ▼
ViewModel (QObject, Q_PROPERTY)
    │
    ├── 属性变化信号 ──→ MainWindow 绑定
    ├── 命令方法 (Q_INVOKABLE)
    │
    ▼
AppEvent (跨页面 Qt 信号中枢)
```

**双事件通道设计:**
- **MessageBus** — 后端事件（跨线程安全，任意线程触发）
- **AppEvent** — UI 层事件（Qt 信号，适用于跨页面通信）

## 组件清单

### 1. ViewModel (`view_model.h`)

UI 状态中心，连接后端事件总线与前端 UI。

**Q_PROPERTY 属性:**

| 属性 | 类型 | 说明 |
|------|------|------|
| `isGrabbing` | `bool` | 是否正在采集 |
| `frameRate` | `double` | 当前帧率 |
| `trackMode` | `int` | 当前跟踪模式 |
| `exposure` | `double` | 当前曝光时间 (ms) |
| `isSaving` | `bool` | 是否正在存储 |
| `isDataExchangeOpen` | `bool` | GXTC/GDCL 数据交换 UDP 是否已显式打开 |
| `networkEndpointConfigs()` | `std::vector<NetworkEndpointState>` | 可编辑网络端点列表，并携带可控服务打开状态 |
| `isNetworkEndpointOpen(key)` | `bool` | 查询图像发送、诊断、大气或心跳服务是否已打开 |
| `serialChannelConfigs()` | `std::vector<SerialChannelState>` | 四路串口配置与注册/打开状态快照 |
| `isSerialChannelOpen(key)` | `bool` | 查询显示、曝光、主控或伺服串口服务是否已打开 |
| `dataExchangeGxtc*` | `QString/int` | GXTC 数据交换本地/远端 IP 与端口 |
| `dataExchangeGdcl*` | `QString/int` | GDCL 数据交换本地/远端 IP 与端口 |
| `statusText` | `QString` | 状态栏文本 |
| `replayFrameCount` | `int` | 当前选择的回放序列帧数 |
| `replayCurrentFrame` | `int` | 当前已显示的回放帧序号（1-based） |

**命令方法 (Q_INVOKABLE):**

| 方法 | 说明 | 当前状态 |
|------|------|---------|
| `selectReplayFiles(files)` | 选择图像序列 | 已接入 `ImageSequenceFrameSource` |
| `startGrab()` | 开始回放/采集 | 已接入 `ImageProcessor` + `IFrameSource` |
| `stopGrab()` | 暂停/停止回放 | 已停止回放源和处理器 |
| `stepReplayForward()` | 单帧前进 | 已接入 `ImageSequenceFrameSource::stepForward()`，可暂停后逐帧浏览 |
| `setProcessingMode(mode)` | 切换处理模式 | 已支持 None/OpenCV，Diff/CUDA 待接入 |
| `setTrackMode(mode)` | 切换跟踪模式 | 已通过策略工厂配置 GEO/Manual/LEO/SC，LEO/SC 算法体仍待迁移 |
| `setExposure(ms)` | 设置曝光时间 | UI 状态已接线，硬件命令待接 |
| `selectTarget(pos)` | 手动选择目标 | 已配置 Manual 策略并发布 `ManualTargetSelectEvent` |
| `startSaving()` | 开始存储 | 已启动图像 raw 与轨迹文本 worker |
| `stopSaving()` | 停止存储 | 已停止并 drain 图像/轨迹存储 worker |
| `applyNetworkEndpointConfig(...)` | 应用单个 UDP 端点配置 | 已覆盖图像发送、诊断、大气、心跳、GXTC/GDCL；不会自动打开 UDP |
| `applyDataExchangeEndpoints(...)` | 应用 GXTC/GDCL 本地/远端 IP 与端口 | 已校验端口范围并更新内存配置；不会自动打开 UDP |
| `openNetworkEndpoint(key)` | 打开可控网络服务 UDP | 已通过 `INetworkChannel` 接入图像发送、诊断、大气接收、心跳；缺失服务会返回错误状态 |
| `closeNetworkEndpoint(key)` | 关闭可控网络服务 UDP | 已通过 `INetworkChannel` 接入图像发送、诊断、大气接收、心跳，并刷新通信页状态 |
| `openSerialChannel(key)` | 打开可控串口通道 | 已通过 `ISerialChannel` 接入显示、曝光、主控、伺服串口；缺失服务会返回错误状态 |
| `closeSerialChannel(key)` | 关闭可控串口通道 | 已通过 `ISerialChannel` 关闭对应串口，并刷新通信页状态 |
| `openDataExchange()` | 打开 GXTC/GDCL 数据交换 UDP | 已接入 `DataExchange`，使用配置中的 `exchangeGxtc`/`exchangeGdcl` |
| `closeDataExchange()` | 关闭 GXTC/GDCL 数据交换 UDP | 已关闭两路 UDP 并更新打开状态 |
| `toggleZoom(level)` | 切换缩放级别 | 事件已发出；滚轮缩放在 `ImageDisplay` 内实现 |

**信号:**
- `displayImageReady(QImage)` — 显示图像就绪
- `cropImageReady(QImage)` — 裁切图像就绪
- `trackInfoUpdated(QString)` — 跟踪信息更新
- `targetListUpdated(int)` — 目标列表更新
- `imageStatsUpdated(...)` — 图像统计更新
- `networkEndpointsChanged()` — 网络端点配置或状态刷新
- `networkServiceStateChanged(QString, bool)` — 可控网络服务打开状态变化
- `serialChannelsChanged()` — 串口通道配置或状态刷新
- `serialChannelStateChanged(QString, bool)` — 可控串口通道打开状态变化
- `dataExchangeOpenChanged(bool)` — GXTC/GDCL 数据交换打开状态变化

**事件订阅:**
- `DisplayRefreshEvent` → `onDisplayRefresh()`
- `ProcessingCompleteEvent` → `onProcessingComplete()`
- `TrackResultEvent` → `onTrackResult()`
- `MasterControlEvent` → `onMasterControl()`
- `NetworkTransmissionErrorEvent` → `onNetworkTransmissionError()`

### 2. AppEvent (`app_event.h`)

跨页面 UI 事件单例（Qt 信号中枢）：

```cpp
class AppEvent : public QObject {
    Q_OBJECT
signals:
    void publishTargetPositionSelected(float x, float y);
    // ...
};
```

用于 MainWindow 内不同页面之间的通信，避免直接引用。

### 3. MainWindow (`main_window.h`)

多页面主窗口：

| 页面 | 旧版 | 说明 |
|------|------|------|
| 控制页 | `UI_CtrlPad` | 采集/跟踪/存储控制 (部分) |
| 显示页 | `UI_DispPad` | 全图显示 |
| 分析页 | — | 图像分析工具 |
| 通信页 | — | 串口/网络状态，串口显式 open/close，网络端点统一编辑，图像发送/诊断/大气/心跳/GXTC/GDCL UDP 联调入口 |
| 设置页 | — | 参数配置 |
| 日志页 | — | 日志查看 |

支持两种窗口后端:
- **ElaWidgetTools** — 现代风格 (`DSS_HAS_ELA=1`)
- **QMainWindow** — Qt 原生回退

### 4. InitDialog (`init_dialog.h`)

启动进度对话框：

| 方法 | 说明 |
|------|------|
| `setStatus(name, ok)` | 设置子系统初始化状态 |
| `setProgress(percent)` | 设置总体进度 |

### 5. ImageDisplay (`image_display.h`)

图像显示控件（从旧版 `QLabelImage` 迁移）：

| 方法 | 说明 |
|------|------|
| `setImage(QImage)` | 更新显示图像 |
| `resetView()` | 回到整图适配视图 |
| 鼠标滚轮 | 围绕鼠标所在图像像素区域放大/缩小 |
| 坐标映射 | Widget 坐标 ↔ 图像坐标 |
| 点击/移动信号 | 用户交互事件 |

### 6. ImageDisplayCrop (`image_display_crop.h`)

裁切放大显示控件（从旧版 `UI_DispPadS` 迁移）：

| 方法 | 说明 |
|------|------|
| `setCropCenter(x, y)` | 设置裁切中心 |
| `setCropSize(w, h)` | 设置裁切区域大小 |

## 旧版对照

| 旧版 | 新版 | 状态 |
|------|------|------|
| `UI_CtrlPad` | `MainWindow` + `ViewModel` | 回放/保存/Manual 选点跟踪首版接线，OpenCV 处理和 GEO/LEO/SC 策略入口已接 |
| `UI_DispPad` | `ImageDisplay` | 基本功能 + 滚轮缩放迁移 |
| `UI_DispPadS` | `ImageDisplayCrop` | 基本功能迁移 |
| `UI_InitDlg` | `InitDialog` | 已迁移 |
| `UI_DistCurve` | — | **未迁移** |
| `QLabelImage` | `ImageDisplay` | 已迁移 |
| 直接子系统引用 | MVVM + 事件总线 | 架构升级 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 当前帧进度 | 已显示序列总帧数和当前帧号，单帧前进按钮已接入；进度条、后退、拖动定位和完整 legacy 浏览行为待补 |
| 处理/跟踪策略选择 | 已接入 None/OpenCV 与 GEO/Manual/LEO/SC；Diff/CUDA、OpenCV 参数和 LEO/SC 算法体仍待迁移 |
| 网络端点 UI 控件 | 图像发送、诊断、大气、心跳、GXTC/GDCL 端点编辑已由统一表单生成；图像发送、诊断、大气、心跳显式 open/close 按钮已通过 `INetworkChannel` 接入 |
| 串口通道 UI 控件 | 显示、曝光、主控、伺服串口已展示端口/波特率/帧长，并通过 `ISerialChannel` 接入显式 open/close；串口参数编辑和协议级联调命令待补 |
| 数据交换 UI 控件 | GXTC/GDCL 显式 open/close 和错误状态提示已接入；日志面板分级展示待补 |
| 距离曲线图 | `UI_DistCurve` 未迁移 |
| 页面布局 | 控制页、显示页、通信页已有首版功能；分析/设置/日志仍是轻量桩实现 |
| 主题/样式 | 未实现自定义样式，依赖 ElaWidgetTools 或默认样式 |

## 依赖关系

```
dss_ui_qt
├── dss_core
├── dss_network_qt
├── dss_processing
├── dss_tracking
├── Qt6::Core
├── Qt6::Gui
├── Qt6::Widgets
├── Qt6::Charts
└── elawidgettools (可选, DSS_HAS_ELA)
```
