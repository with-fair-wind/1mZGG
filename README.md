# DSS_QT

DSS_QT 是面向天文观测的 C++23 / Qt 6 桌面系统，覆盖高帧率图像采集、处理、目标跟踪、异步存储以及串口/UDP 通信。

旧版 qmake/OpenCL 单体代码保存在 `oldsrc/`，仅作为只读归档，不参与构建。当前开发以 CMake 模块、现代 C++ 接口和自动化测试为事实来源。

## 许可与访问范围

Copyright (c) 2026 kk。项目自有代码采用 [DSS_QT 私有非商业使用许可](LICENSE)（`LicenseRef-DSS-QT-Private-Noncommercial-1.0`），保留所有权利；这不是标准开源许可证。

只有取得明确书面授权的指定对象，才可在授权范围内用于个人学习或非商业研究。未经另行书面许可，不得用于商业活动、收费服务、销售或再分发，也不得公开发布源码、二进制或修改版。仅取得仓库访问权、下载或 CI 产物不等于取得使用授权；全文以 [LICENSE](LICENSE) 为准。

第三方库、SDK 和归档片段保留各自版权及条款，见 [第三方许可说明](THIRD_PARTY_NOTICES.md)。当前构建仍声明 Qt Charts，添加本项目许可不等于已解决 GPLv3 组合或其他依赖的对外发行授权。许可文件不会自动改变仓库和 CI 产物的可见性；对外授权及发行前应确认第三方兼容性，并复核具体法律条款。

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

头文件统一放在 `include/dss/<模块>/`，模块内部实现声明放在对应的 `detail/` 子目录；`src/<模块>/` 存放 `.cpp` 实现。生产代码和测试均使用 `dss/...` 路径引用头文件。

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
| `CMAKE_DISABLE_PRECOMPILE_HEADERS` | 普通预设 OFF；质量 CI ON | 控制已配置目标是否使用 PCH |

项目已通过 `dss_std_pch` 为核心库配置标准库 PCH，通过 `dss_qt_pch` 为 Qt 模块和主程序追加常用 QtCore 头。普通开发预设明确启用 PCH，质量预设 `ci-clang-quality` 关闭 PCH，以暴露被预编译头掩盖的依赖。PCH 由 CMake 在构建目录生成，各目标私有使用，不作为公共接口传播；测试与独立头文件检查不依赖 PCH。可通过 `cmake --preset clang-cl-debug -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` 临时关闭，普通预设下次重新配置会恢复启用；不额外引入项目开关或跨目标 PCH 复用。

Sapera/CUDA 不使用默认开发 preset 验收；请严格按 [硬件验证手册](docs/hardware-validation.md) 执行并回填结果。

## 代码质量

```powershell
cmake --build --preset clang-cl-debug --target format-check
cmake --build --preset clang-cl-debug --target tidy-check
```

项目遵循 `.clang-format`、`.clang-tidy` 和现代 C++ RAII/窄接口约定。`format`/`tidy` 会修改代码，带 `-check` 的目标只检查；`tidy-check` 默认将 warning 作为失败条件，已有构建目录需重新配置或显式设置 `DSS_CLANG_TIDY_WARNINGS_AS_ERRORS=ON`。clangd 共用 `.clang-tidy` 规则。检查器版本不同仍可能产生不同诊断；本机 Scoop 升级后，应先用 CI 固定版本复现，再通过版本更新提交提升质量基线。

`format-check` 检查受 Git 跟踪的项目 C/C++ 文件；`tidy-check` 先检查实际被当前目标使用的项目头文件，将其逐个作为主文件运行 Include Cleaner。通过后，再按共享规则分析编译数据库中的 `src`、`tests`、`tools` 和 `benchmarks` 翻译单元；仅扫描 `.cpp` 不能发现被包含头文件中的冗余 include。

完整检查需要 Python 3.9+ 和已完成全量构建的 Ninja 目录；缺失对象依赖或 Ninja 报告为过期的依赖记录会在源码分析之前使检查失败，并输出当前构建目录对应的全量构建命令。头文件使用实际消费它的编译命令，临时数据库只移除 PCH 和输出参数，不补造宏或搜索路径。每个活动头文件选取一个实际消费目标的上下文，不代表检查了全部条件编译组合。

旧代码、第三方及生成文件不作为扫描入口；编译数据库之外的 CUDA/Sapera 可选硬件配置需另行验收。编辑器与门禁均检查冗余 include，包括非标准库的尖括号 include；缺失 include 检查仍关闭。正常 C++ 源文件统一使用 `.h`/`.cpp`，私有头文件也应显式包含依赖并在文件内声明命名空间。必要的平台前置头和公开类型转导出使用有原因的 IWYU 注释。日志中的 `N warnings generated` 包含随后被过滤的第三方诊断和局部例外，应以实际显示的项目诊断及进程退出码判断门禁结果。

复杂度规则忽略宏展开及宏参数（包括 GoogleTest 的测试体），普通函数的阈值仍为 30。Qt 槽标记、公共枚举 ABI 和必要的平台转换使用带原因的局部例外，不关闭整类检查；`tidy` 的自动修改必须审查并运行回归测试，特别注意引用类别、NaN 比较和 Qt 元对象。

`.github/workflows/quality.yml` 在推送、PR 和手动触发时运行独立质量门禁：LLVM 23.1.3、Conan 2.32.0、CMake 4.4.3、Ninja 1.13.2、Qt 6.8.0；启用默认 Qt/OpenCV 和测试，使用 Ninja 并禁用 PCH，先验证格式与规则配置，执行真实 Git/Ninja 小项目回归（验证 clang-cl/clang++ 的 PCH 参数处理、未构建时提前停止，以及仅头文件中的冗余 include 会使门禁失败、清理后通过），再构建并运行 `format-check`/`tidy-check`。默认配置当前覆盖 140 个源文件及 103 个活动头文件；4 个未启用的 GPU 头文件会明确报告为未检查。失败日志与工具版本保留 14 天。工具升级通过版本更新提交验证，最新稳定工具链兼容性任务继续独立运行。

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
