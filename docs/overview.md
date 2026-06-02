# DSS_QT 系统架构总览

> 天文高帧率图像处理系统 (Astronomical High Frame-Rate Image Processing System)

## 项目概述

DSS_QT 是一套用于天文观测的实时图像采集、处理、跟踪与通信系统。系统通过高帧率相机采集星空图像，经过图像处理管线（统计分析、阈值分割、连通域检测）提取目标信息，再由跟踪算法（GEO/LEO/SC/Manual 四种模式）计算目标轨迹，最终将修正数据通过串口/网络发送给伺服系统和上位机。

当前正在从旧版 qmake/OpenCL 单体代码（`oldsrc/`）向现代 CMake 模块化架构迁移。

## 技术栈

| 类别 | 技术选型 |
|------|---------|
| 语言标准 | C++23 |
| 构建系统 | CMake 3.28+、CMakePresets |
| 包管理 | Conan 2 |
| UI 框架 | Qt 6 (Widgets / Charts) |
| 图像处理 | OpenCV 4.10 (可选)、CUDA (可选) |
| 测试框架 | Google Test |
| 日志 | spdlog + 事件总线 |
| 配置 | nlohmann_json (JSON) |
| 代码质量 | clang-format、clang-tidy、IWYU |
| CI | GitHub Actions (MSVC Release) |

## 架构设计原则

1. **分层模块化** — 9 个静态库 CMake target，显式依赖方向
2. **Qt 仅在边界** — UI、串口、UDP 适配层使用 Qt，核心逻辑 Qt-free
3. **策略模式** — 图像处理 (`IProcessingStrategy`) 和跟踪 (`ITrackingStrategy`) 通过接口解耦
4. **事件驱动解耦** — `BasicMessageBus`（后端）+ `AppEvent`（跨 UI 页面 Qt 信号）
5. **依赖注入** — `ServiceRegistry` + `ServiceHost` 管理服务生命周期
6. **旧代码仅参考** — `oldsrc/` 不参与构建，不被 clangd 索引

## 模块依赖图

```
DSS_QT (可执行文件)
├── dss_ui_qt       → dss_core, dss_processing, dss_tracking, Qt6
├── dss_app         → dss_core, dss_comm_qt, dss_network_qt
├── dss_comm_qt     → dss_core, Qt6::SerialPort
├── dss_network_qt  → dss_core, Qt6::Network
├── dss_processing  → dss_core, dss_tracking
├── dss_processing_opencv (可选) → dss_processing, OpenCV
├── dss_tracking    → dss_core
├── dss_gpu_cuda (可选) → dss_core, CUDA
└── dss_core        → nlohmann_json
```

## 数据流概览

```
相机采集              串口同步               图像处理              跟踪算法
┌──────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ IFrameSource │ ──→ │ ExposureChannel │ ──→ │ImageProcessor│ ──→ │ TrackManager │
│ (Grabber)    │     │ DisplayChannel  │     │ Pipeline     │     │ GEO/LEO/SC   │
└──────────┘     └──────────────┘     └──────────────┘     └──────────────┘
                                            │                       │
                                            ▼                       ▼
                                     ┌──────────────┐     ┌──────────────┐
                                     │  显示刷新      │     │ 伺服修正      │
                                     │ DisplayRefresh│     │ ServoChannel │
                                     │ → MainWindow  │     │ → 串口发送    │
                                     └──────────────┘     └──────────────┘
                                                                  │
                                                                  ▼
                                                          ┌──────────────┐
                                                          │ 网络上报      │
                                                          │ ImageSender  │
                                                          │ DataExchange │
                                                          │ Heartbeat    │
                                                          └──────────────┘
```

## 目录结构

```
DSS_QT/
├── cmake/                  CMake 辅助脚本 (clang-format/clang-tidy)
├── config/                 运行时配置 (SystemInit.json)
├── docs/                   项目文档 (本目录)
├── include/dss/            公共头文件
│   ├── acquisition/        采集层 (相机控制接口)
│   ├── app/                应用层 (组合根)
│   ├── comm/               串口通信层
│   ├── core/               核心层 (类型、事件、配置、服务)
│   ├── gpu/                GPU 计算层
│   ├── network/            网络通信层
│   ├── processing/         图像处理层
│   ├── storage/            存储层
│   ├── tracking/           跟踪算法层
│   └── ui/                 UI 层
├── kernels/                CUDA 核函数 (.cu)
├── oldsrc/                 旧版代码 (仅参考，不构建)
├── profiles/conan/         Conan 编译器配置
├── src/                    源文件 (与 include 结构对应)
├── tests/                  单元测试 (GTest)
├── CMakeLists.txt          根构建文件
├── CMakePresets.json        构建预设
└── conanfile.py            Conan 依赖声明
```

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `DSS_BUILD_APP` | ON | 构建 Qt 桌面应用程序 |
| `DSS_ENABLE_TESTS` | ON | 构建单元测试 |
| `DSS_ENABLE_CUDA` | OFF | 构建 CUDA GPU 后端 |
| `DSS_ENABLE_OPENCV` | ON | 构建 OpenCV 图像处理后端 |
| `DSS_ENABLE_SAPERA` | OFF | 使用 Sapera SDK (相机采集卡) |
| `DSS_ENABLE_STARLIBS` | OFF | 使用 StarMap/Photometry 库 |
| `DSS_ENABLE_QT_DEPLOY` | ON (Win) | 构建后运行 windeployqt |

## 命名规范

| 元素 | 风格 | 示例 |
|------|------|------|
| 文件名 | snake_case | `image_processor.h` |
| 命名空间 | Dss::Module | `Dss::Core`、`Dss::Processing` |
| 类/结构体 | PascalCase | `ImageProcessor`、`FramePacket` |
| 方法/函数 | camelCase | `submitFrame()`、`decodeDisplayFrame()` |
| 成员变量 | m_ 前缀 | `m_bus`、`m_frameChannel` |
| 常量 | PascalCase / UPPER_CASE | `FrameHeader`、`DegToRad` |
| 枚举 | enum class + PascalCase 值 | `TrackMode::Geo` |

## 模块文档索引

| 文档 | 说明 |
|------|------|
| [Core 模块](module-core.md) | 类型系统、事件总线、配置、服务注册 |
| [App 模块](module-app.md) | 应用上下文、组合根、服务编排 |
| [Processing 模块](module-processing.md) | 图像处理管线、帧模型、策略 |
| [Tracking 模块](module-tracking.md) | 跟踪算法、数学工具 |
| [Comm 模块](module-comm.md) | 串口通信、协议编解码 |
| [Network 模块](module-network.md) | UDP 网络通信、图像传输 |
| [Storage 模块](module-storage.md) | 图像/轨迹数据存储格式 |
| [Acquisition 模块](module-acquisition.md) | 相机采集接口、控制协议 |
| [GPU 模块](module-gpu.md) | CUDA 设备管理、GPU 核函数 |
| [UI 模块](module-ui.md) | 视图模型、主窗口、图像显示 |
| [迁移进度](migration-status.md) | oldsrc → 新架构迁移状态 |
