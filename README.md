# DSS_QT

DSS_QT 是面向天文观测的 C++23 / Qt 6 桌面系统，覆盖高帧率图像采集、处理、目标跟踪、异步存储以及串口/UDP 通信。

旧版 qmake/OpenCL 单体代码保存在 `oldsrc/`，仅作为只读归档，不参与构建。当前开发以 CMake 模块、现代 C++ 接口和自动化测试为事实来源。

## 当前状态

主架构与无硬件业务闭环已经落地：

- CMake/CMakePresets、Conan 2、C++23 和模块化静态库。
- 图像序列回放，支持暂停、前进/后退、随机定位和运行中 seek。
- Direct、OpenCV、CPU Diff 处理策略；CUDA 为显式启用的可选策略。
- GEO、LEO、SC、Manual 四种跟踪模式及 legacy JSON 黄金场景。
- RAW/BMP、IMI/GAE 会话写入、背压、错误事件和任务到会话命名映射。
- 显示、曝光、主控、伺服串口，以及图像、心跳、诊断、大气和 GXTC/GDCL 网络服务。
- Qt MVVM UI、文件日志、运行诊断和默认无硬件安全启动。
- 可选 Sapera 实时帧源、CUDA 正确性测试和性能基准入口。

仍需现场或产品决策的事项：

- Sapera 采集机 smoke test。
- CUDA 设备输出对照、6144 级性能收益及 UI 启用门槛。
- 相机 `SerialCameraController` 已实现并测试，但默认组合根仍注册 `CommandOnlyCameraController`，不会打开相机串口。
- Manual 完整 legacy 历史校验、光度/星图/完整定标、串口断连重连、距离曲线 UI。
- `SingleApplication` 与 ENet 尚未迁移，当前主链路不依赖它们。

完整状态见 [迁移状态](docs/migration-status.md)，硬件步骤见 [硬件验证手册](docs/hardware-validation.md)。

## 架构

```text
IFrameSource (Replay / Sapera)
        │
        ▼
ImageProcessor ── ProcessingPipeline (Direct / OpenCV / Diff / CUDA)
        │
        ├── DisplayRefreshEvent ──► MainViewModel / MainWindow
        ├── ITrackingStrategy ────► GEO / LEO / SC / Manual
        └── StorageBackend ───────► RAW/BMP/IMI/GAE
                                      │
TrackResultEvent ──► DataExchange ────┴──► GXTC/GDCL
```

主要 CMake target：

| Target | 职责 |
|---|---|
| `dss_core` | 类型、配置、事件总线、服务注册、日志及存储 |
| `dss_app` | 组合根、服务编排、诊断和结果桥接 |
| `dss_tracking` | GEO/LEO/SC/Manual 策略与数学 helper |
| `dss_processing` | 帧模型、工作线程、Pipeline、Direct/Diff 与 Labeler |
| `dss_processing_opencv` | 可选 OpenCV 参考后端 |
| `dss_gpu_cuda` | 可选 CUDA 处理策略、设备和 kernels |
| `dss_acquisition_qt` | 回放、帧源协调、相机控制和可选 Sapera |
| `dss_comm_qt` | Qt 串口通道与协议适配 |
| `dss_network_qt` | Qt UDP 服务与协议适配 |
| `dss_ui_qt` | MainWindow、子 ViewModel 和显示控件 |
| `DSS_QT` | 桌面应用可执行文件 |

`ApplicationContext::registerCommunicationServices()` 负责集中注册服务。构造和注册不会自动打开串口、UDP 或采集设备。

## 环境要求

- CMake 3.28+
- Conan 2
- Ninja
- C++23 编译器
- Qt 6（构建桌面应用时需要，路径通过 `DSS_QT_ROOT` 提供）
- 可选：OpenCV 4.10（Conan）、CUDA Toolkit、Sapera SDK

Windows 支持 MSVC、clang-cl 和 MinGW preset；Linux 提供 GCC/Clang preset。使用 `cmake --list-presets` 查看当前可用项。

## 快速开始

以下示例使用 Windows clang-cl Debug：

```powershell
$env:DSS_QT_ROOT = 'D:\Qt\6.8.0\msvc2022_64'

conan install . --build=missing `
  -pr:h profiles/conan/windows-clang-cl -pr:b default `
  -s:h build_type=Debug -s:h compiler.runtime_type=Debug `
  --output-folder=build/clang-cl-debug

cmake --preset clang-cl-debug --fresh
cmake --build --preset clang-cl-debug
ctest --test-dir build/clang-cl-debug --output-on-failure
```

应用位于 `build/clang-cl-debug/bin/DSS_QT.exe`。MSVC 使用 `profiles/conan/windows-msvc` 与对应的 `msvc-debug` / `msvc-release` preset。

仅构建不依赖 Qt 的核心测试：

```powershell
conan install . --build=missing `
  -pr:h profiles/conan/windows-clang-cl -pr:b default `
  -s:h build_type=Debug -s:h compiler.runtime_type=Debug `
  -o:h '&:with_opencv=False' `
  --output-folder=build/core-tests

cmake --preset core-tests --fresh
cmake --build --preset core-tests
ctest --test-dir build/core-tests --output-on-failure
```

## 构建选项

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `DSS_BUILD_APP` | ON | 构建 Qt 桌面应用 |
| `DSS_ENABLE_TESTS` | ON | 构建测试 |
| `DSS_ENABLE_OPENCV` | ON | 构建 OpenCV 后端 |
| `DSS_ENABLE_CUDA` | OFF | 构建 CUDA 策略与 kernels |
| `DSS_ENABLE_SAPERA` | OFF | 构建 Sapera SDK 适配器 |
| `DSS_ENABLE_STARLIBS` | OFF | 链接 StarMap/Photometry 库 |
| `DSS_ENABLE_QT_DEPLOY` | Windows ON | 构建后运行 `windeployqt` |

Sapera/CUDA 不使用默认开发 preset 验收；请严格按 [硬件验证手册](docs/hardware-validation.md) 执行并回填结果。

## 代码质量

```powershell
cmake --build --preset clang-cl-debug --target format-check
cmake --build --preset clang-cl-debug --target tidy-check
```

项目遵循 `.clang-format`、`.clang-tidy` 和现代 C++ RAII/窄接口约定。`oldsrc/` 不在 clangd 索引或任何 CMake target 中。

## 文档索引

| 文档 | 内容 |
|---|---|
| [系统总览](docs/overview.md) | 架构、数据流和目录结构 |
| [迁移状态](docs/migration-status.md) | 当前完成项、真实缺口和验收基线 |
| [硬件验证](docs/hardware-validation.md) | Sapera/CUDA 命令、门槛和结果表 |
| [Legacy 能力映射](docs/legacy-capability-map.md) | 旧能力到现代模块/测试的索引 |
| [Core](docs/module-core.md) | 类型、配置、事件和服务 |
| [Application](docs/module-app.md) | 组合根和服务注册 |
| [Acquisition](docs/module-acquisition.md) | 回放、相机控制和 Sapera |
| [Processing](docs/module-processing.md) | 帧处理与策略后端 |
| [Tracking](docs/module-tracking.md) | 四种跟踪模式和公共 helper |
| [Storage](docs/module-storage.md) | 图像/轨迹会话存储 |
| [Communication](docs/module-comm.md) | 串口协议与通道 |
| [Network](docs/module-network.md) | UDP 服务和数据交换 |
| [GPU](docs/module-gpu.md) | CUDA 策略、设备和 kernels |
| [UI](docs/module-ui.md) | MainWindow 与子 ViewModel |
| [旧新架构阅读笔记](docs/legacy-modern-migration-note.md) | 面向代码阅读的旧新对照指南 |
