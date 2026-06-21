# UI 模块 (`dss_ui_qt`)

> 命名空间: `Dss::Ui`
>
> 头文件: `include/dss/ui/`
>
> 源文件: `src/ui/`
>
> 依赖: `dss_core`, `dss_app`, `dss_acquisition_qt`, `dss_network_qt`, `dss_processing`, `dss_tracking`, `Qt6::Core/Gui/Widgets/Charts`

## 模块职责

UI 模块实现系统的图形用户界面，采用 MVVM 模式：`MainViewModel` 作为 UI 层主 ViewModel 与组合根创建各业务域子 ViewModel，`MainWindow` 及各页面组件直接绑定到对应子模块的属性和信号。

## 架构模式

```
事件总线 (MessageBus)
    │
    ▼
MainViewModel (QObject 主 ViewModel / 组合根)
    │
    ├── Replay/Display/Processing/Tracking/Storage
    ├── SerialPort/Network/DataExchange/Log
    ├── 状态汇总信号 ──→ MainWindow 状态栏
    │
    ▼
AppEvent (跨页面 Qt 信号中枢)
```

**双事件通道设计:**
- **MessageBus** — 后端事件（跨线程安全，任意线程触发）
- **AppEvent** — UI 层事件（Qt 信号，适用于跨页面通信）

## 组件清单

### 1. UI 主 ViewModel 与子 ViewModel (`main_view_model.h` + `*_view_model.h`)

`MainViewModel` 是 UI 层主 ViewModel 与组合根，只负责创建子模块、汇总状态文本、连接少量跨模块关系，并响应 `MasterControlEvent` 协调曝光、跟踪、保存和回放状态。业务状态与命令由具体子 ViewModel 承担，旧单体 `ViewModel` facade 已从代码和构建中移除。

| 子模块 | 主要职责 |
|------|---------|
| `ReplayViewModel` | 选择图像序列、初始化 `ImageSequenceFrameSource`、开始/暂停、单帧前进、维护当前帧和总帧数 |
| `DisplayViewModel` | 缓存当前 raw frame、计算统计信息、自动/手动拉伸、实时刷新显示图和裁切图 |
| `ProcessingViewModel` | 同步 None/OpenCV/Diff 处理策略与参数快照；CUDA 仅在硬件收益达标后开放 |
| `TrackingViewModel` | 同步 Manual/GEO/LEO/SC 跟踪策略、处理手动选点、输出跟踪状态与目标信息 |
| `StorageViewModel` | 控制图像 raw worker 与轨迹文本 worker 的启动、停止和 drain |
| `SerialPortViewModel` | 管理显示、曝光、主控、伺服串口配置、显式 open/close 和联调命令 |
| `NetworkViewModel` | 管理图像发送、诊断、大气、心跳 UDP 端点配置和显式 open/close |
| `DataExchangeViewModel` | 管理 GXTC/GDCL 数据交换端点、显式 open/close 和端点重配关闭保护 |
| `LogViewModel` | 缓存 spdlog 事件、网络/串口错误事件，支持 Info/Warning/Error 分级过滤和彩色显示 |

**聚合根信号:**
- `statusTextChanged(QString)` — 子模块状态文本汇总后通知状态栏
- `exposureChanged(double)` — 主控或 UI 修改曝光时间后通知控件同步

**典型事件订阅归属:**
- `DisplayRefreshEvent` → `DisplayViewModel`
- `ProcessingCompleteEvent` → `DisplayViewModel`
- `TrackResultEvent` → `TrackingViewModel` / `StorageViewModel`
- `MasterControlEvent` → `MainViewModel`
- `NetworkTransmissionErrorEvent` → `LogViewModel`
- `SerialFrameErrorEvent` → `LogViewModel`
- `SerialDecodeErrorEvent` → `LogViewModel`
- `LogMessageEvent` → `LogViewModel`

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
| 通信页 | — | 串口/网络状态，串口显式 open/close，串口曝光/伺服/主控状态联调命令，网络端点统一编辑，图像发送/诊断/大气/心跳/GXTC/GDCL UDP 联调入口 |
| 设置页 | — | 路径、滚动日志和光学参数校验与持久化 |
| 日志页 | — | 分级过滤、彩色显示、搜索和导出核心日志及通信/存储错误，最多缓存最近 500 条 |

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
| `UI_CtrlPad` | `MainWindow` + `MainViewModel` + 子 ViewModel | 回放定位、保存、运行诊断、参数化处理及四种跟踪入口已接 |
| `UI_DispPad` | `ImageDisplay` | 基本功能 + 滚轮缩放迁移 |
| `UI_DispPadS` | `ImageDisplayCrop` | 基本功能迁移 |
| `UI_InitDlg` | `InitDialog` | 已迁移 |
| `UI_DistCurve` | — | **未迁移** |
| `QLabelImage` | `ImageDisplay` | 已迁移 |
| 直接子系统引用 | MVVM + 事件总线 | 架构升级 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| CUDA 模式 | 可选后端和基准入口已存在；达到 [硬件验证](hardware-validation.md) 的收益门槛后再加入处理模式选择 |
| 串口/网络联调状态 | 参数编辑、显式开关、错误日志和首批命令已接入；发送样例、接收状态和断连重连展示仍需完善 |
| 距离曲线图 | `UI_DistCurve` 未迁移，分析页仅保留入口空间 |

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
