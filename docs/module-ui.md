# UI 模块 (`dss_ui_qt`)

> 命名空间: `Dss::Ui`
>
> 头文件: `include/dss/ui/`
>
> 源文件: `src/ui/`
>
> 依赖: `dss_core`, `dss_processing`, `dss_tracking`, `Qt6::Core/Gui/Widgets/Charts`

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
| `statusText` | `QString` | 状态栏文本 |
| `replayFrameCount` | `int` | 当前选择的回放序列帧数 |

**命令方法 (Q_INVOKABLE):**

| 方法 | 说明 | 当前状态 |
|------|------|---------|
| `selectReplayFiles(files)` | 选择图像序列 | 已接入 `ImageSequenceFrameSource` |
| `startGrab()` | 开始回放/采集 | 已接入 `ImageProcessor` + `IFrameSource` |
| `stopGrab()` | 暂停/停止回放 | 已停止回放源和处理器 |
| `setTrackMode(mode)` | 切换跟踪模式 | UI 状态已接线，策略算法待迁移 |
| `setExposure(ms)` | 设置曝光时间 | UI 状态已接线，硬件命令待接 |
| `selectTarget(pos)` | 手动选择目标 | 已发布 `ManualTargetSelectEvent` |
| `startSaving()` | 开始存储 | 已启动 `LocalImageStorageBackend` worker |
| `stopSaving()` | 停止存储 | 已停止并 drain 存储 worker |
| `toggleZoom(level)` | 切换缩放级别 | 事件已发出；滚轮缩放在 `ImageDisplay` 内实现 |

**信号:**
- `displayImageReady(QImage)` — 显示图像就绪
- `cropImageReady(QImage)` — 裁切图像就绪
- `trackInfoUpdated(QString)` — 跟踪信息更新
- `targetListUpdated(int)` — 目标列表更新
- `imageStatsUpdated(...)` — 图像统计更新

**事件订阅:**
- `DisplayRefreshEvent` → `onDisplayRefresh()`
- `ProcessingCompleteEvent` → `onProcessingComplete()`
- `TrackResultEvent` → `onTrackResult()`
- `MasterControlEvent` → `onMasterControl()`

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
| 通信页 | — | 串口/网络状态 |
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
| `UI_CtrlPad` | `MainWindow` + `ViewModel` | 回放/保存/模式控制首版接线 |
| `UI_DispPad` | `ImageDisplay` | 基本功能 + 滚轮缩放迁移 |
| `UI_DispPadS` | `ImageDisplayCrop` | 基本功能迁移 |
| `UI_InitDlg` | `InitDialog` | 已迁移 |
| `UI_DistCurve` | — | **未迁移** |
| `QLabelImage` | `ImageDisplay` | 已迁移 |
| 直接子系统引用 | MVVM + 事件总线 | 架构升级 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 当前帧进度 | 已显示序列帧数，当前帧号/进度条尚未接线 |
| 处理/跟踪策略选择 | UI 模式值已接线，Manual/GEO/LEO/SC 算法体尚待迁移 |
| 距离曲线图 | `UI_DistCurve` 未迁移 |
| 页面布局 | MainWindow 各页面为桩实现 |
| 主题/样式 | 未实现自定义样式，依赖 ElaWidgetTools 或默认样式 |

## 依赖关系

```
dss_ui_qt
├── dss_core
├── dss_processing
├── dss_tracking
├── Qt6::Core
├── Qt6::Gui
├── Qt6::Widgets
├── Qt6::Charts
└── elawidgettools (可选, DSS_HAS_ELA)
```
