# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

DSS_QT 是面向天文观测的 C++23 / Qt 6 桌面系统：高帧率相机采集 → 图像处理（统计/阈值/连通域）→ 跟踪（GEO/LEO/SC/Manual）→ 串口/UDP 通信与异步存储。`oldsrc/` 是旧 qmake/OpenCL 单体的只读归档，**不参与任何 CMake target、不被 clangd 索引**，仅作行为对照。

深度架构、数据流、调用栈、线程/所有权表见 `docs/overview.md`；本文件只记录需要跨多文件阅读才能掌握的要点和可复制命令。

## 构建与测试

环境要求：CMake 3.28+、Conan 2、Ninja、C++23 编译器、Qt 6（经 `DSS_QT_ROOT` 提供）。Conan 依赖：`gtest/1.15.0`、`spdlog/1.14.0`、`nlohmann_json/3.11.3`、`opencv/4.10.0`（可选）。可用 preset 列表：`cmake --list-presets`。

Windows clang-cl Debug（README 的规范流程）：

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

应用产物：`build/<preset>/bin/DSS_QT.exe`。MSVC 用 `profiles/conan/windows-msvc` + `msvc-debug`/`msvc-release` preset。CI 用 `ci-msvc-release`（Visual Studio 多配置生成器）。

仅构建不依赖 Qt 的核心测试（`core-tests` preset，关 app/cuda/qt）：

```powershell
conan install . --build=missing -pr:h profiles/conan/windows-clang-cl -pr:b default `
  -s:h build_type=Debug -s:h compiler.runtime_type=Debug `
  -o:h '&:with_opencv=False' --output-folder=build/core-tests
cmake --preset core-tests --fresh && cmake --build --preset core-tests
ctest --test-dir build/core-tests --output-on-failure
```

**跑单个测试**（test 名 == 可执行名，见 `tests/CMakeLists.txt`；优先用 ctest 以保证 Qt 测试的 PATH 注入）：

```powershell
ctest --test-dir build/clang-cl-debug -R test_image_processor --output-on-failure
```

**代码质量**（目标由 `cmake/DssTooling.cmake` 提供，仅非多配置生成器启用 tidy）：

```powershell
cmake --build --preset clang-cl-debug --target format-check   # 仅检查
cmake --build --preset clang-cl-debug --target format         # 就地修复
cmake --build --preset clang-cl-debug --target tidy-check
cmake --build --preset clang-cl-debug --target tidy           # --fix 就地修复
```

测试用 GTest：`dss_add_test`（核心，`gtest_discover_tests`）与 `dss_add_qt_test`（Qt 测试，在 Windows 把 Qt bin 前置到 PATH）。添加测试时按模块链接到对应 `dss_*` target。

## 架构要点

**分层与依赖方向**（`CMakeLists.txt` 的 `target_link_libraries` 是事实来源）：`dss_core`（含 storage，编译进 core）→ `dss_tracking`、`dss_processing` → `dss_app`（组合根，`DSS_BUILD_APP` 时私有连 comm/network/processing/acquisition）→ `dss_ui_qt` + `DSS_QT`。可选 target：`dss_processing_opencv`、`dss_gpu_cuda`、`dss_sapera_smoke`、`dss_processing_benchmark`。

**Qt 仅在边界**：UI、串口、UDP 适配用 Qt，核心逻辑 Qt-free。不要让核心类型依赖 Qt 头。

**组合根** `Dss::App::ApplicationContext`（`include/dss/app/application_context.h`）持有 `MessageBus` 与 `ServiceRegistry` 两个成员。`main.cpp` 顺序：`wireLogger()` → `loadConfig()`（读 `../config/SystemInit.json`，构建时由 POST_BUILD 拷贝 `config/`→`build/config`）→ `registerCommunicationServices()`（受 `DSS_BUILD_APP` 保护）→ 构造 `MainViewModel(bus, registry)` 与 `MainWindow`。**注册只创建对象，不自动打开串口/UDP/相机**——这些生命周期由对应 ViewModel 显式驱动（`startGrab()`/`open...()`/`startSaving()`），并由服务内部标准库锁串行化启停。

**服务定位** `Dss::Core::ServiceRegistry`：按 `(接口 type_index, 名称)` 存 `shared_ptr`。`get<T>()` 未注册时 **抛 `std::runtime_error`**，`tryGet<T>()` 返回 nullptr。所有服务的规范名称集中在 `include/dss/app/service_keys.h`（`Dss::App::ServiceKey` 命名空间，如 `imageProcessor`、`replaySource`、`servo`、`dataExchange`）——新增/查询服务一律用这里的常量，不要散落字符串。

**事件总线** `Dss::Core::MessageBus` = `Evt::BasicMessageBus<SharedMutexLock>`：`subscribe<Message>(fn)` 返回 `ScopedConnection`（RAII 自动退订，常存于 `ApplicationContext`/ViewModel 成员），`emit<Message>(msg)` **同步**在调用者线程触发所有订阅者，`post()`+`flush()` 为异步。关键语义：主帧事件由 `ImageProcessor` 工作线程 emit，因此订阅者若触碰 Qt 控件**必须先 marshal 到控件所属线程**（多数 ViewModel 只产出值/发 Qt 信号，扩展时逐个核对线程亲和性）。事件类型定义在 `include/dss/core/event/events.h`（`DisplayRefreshEvent`/`ProcessingCompleteEvent`/`TrackResultEvent` 等）。

**两套解耦机制并存**：跨线程/业务用 `MessageBus`；跨 UI 页面的纯前端事件用 `Dss::Ui::AppEvent` 单例（`targetPositionSelected`/`zoomLevelChanged` 等 Qt 信号）。

**帧数据所有权**：RAW 帧自采集起以 `shared_ptr<const vector<uint16_t>>` 跨处理/显示/存储共享；GPU 显示只接收 RAW + low/high，8 位网络图由 `ImageReadyForSendEvent::imageFactory` 按需生成。修改帧路径时保持这一共享不变量。

**UI 工作区**：`MainWindow` 把功能页作为原生 `QDockWidget` 管理（浮动/停靠/隐藏恢复），窗口几何与 Dock 状态经 `QSettings` 持久化（组织名 `DPS`、应用名 `DSS_QT`），恢复失败回退默认顶部 Tab。布局变化只改 Dock 状态，不重建业务页面。

## 关键约定与陷阱

- **命名**：文件 `snake_case.h/.cpp`；命名空间 `Dss::{Core,App,Processing,Tracking,Evt,Ui,...}`；类 PascalCase；方法 camelCase；成员 `m_` 前缀；枚举 `enum class` + PascalCase 值（如 `TrackMode::Geo`）。`src/` 与 `include/dss/` 目录结构一一对应。
- **错误处理**：配置/加载类返回 `std::expected<void, std::string>`（`<expected>` 在 PCH 中）；服务查找用异常。新增加载逻辑优先沿用 `std::expected`。
- **`QT_NO_EMIT`**：根 `CMakeLists.txt` 全局定义该宏，Qt 的 `emit` 关键字被置空。事件总线头 `event_bus.h` 内部 `#undef emit` 再恢复，使其 `bus.emit(msg)` / `Event::emit()` 作为真实方法可用——所以事件总线代码无需特殊处理；但**不要在 Qt 信号处依赖 `emit` 关键字**，用 `Q_EMIT` 或直接调用信号。
- **预编译头**：`dss_std_pch`（标准库常用头）与 `dss_qt_pch`（额外 `QObject`/`QDebug`/`QString`）。新 target 用对应 `dss_*_pch` 包装。
- **编译器警告**：MSVC `/W4 /permissive- /Zc:__cplusplus /utf-8 /wd4100`；Clang/GCC `-Wall -Wextra -Wpedantic -Wno-unused-parameter`。
- **clangd**：`.clangd` 把 `oldsrc|third_party|build|.ref-repos` 排除出索引并抑制其诊断；配置后 `dss_sync_compile_commands` target 把 `compile_commands.json` 镜像到源码根供 clangd 使用（`DSS_QT` 依赖它）。
- **构建开关**：`DSS_ENABLE_CUDA`/`DSS_ENABLE_SAPERA`/`DSS_ENABLE_STARLIBS` 默认 OFF，对应宏 `DSS_HAS_CUDA`/`DSS_HAS_SAPERA`/`DSS_HAS_OPENGL_WIDGETS`/`DSS_HAS_ELA`（`third_party/ElaWidgetTools` 为可选子目录）。阅读某条调用链前先确认对应 target/宏是否存在。Sapera/CUDA 的验收须按 `docs/hardware-validation.md` 执行，**不在默认开发 preset 内**。
- **`windeployqt`**：`DSS_ENABLE_QT_DEPLOY` 在 Windows 默认 ON，构建后自动部署 Qt 运行时到 `bin/`。
- **Windows 工作环境**：本机的 Bash 工具不带 `ls`/`find`/`head`/`grep` 等 Unix 工具——目录列举用 Glob，内容检索用 Grep，文件读取/编辑用 Read/Edit，终端命令用 PowerShell 工具。

## 文档导航

`docs/overview.md`（架构总览 + 按源码还原的阅读地图/调用栈/线程表）、`docs/migration-status.md`（完成项与真实缺口）、`docs/hardware-validation.md`（Sapera/CUDA 验收）、`docs/legacy-capability-map.md`（旧→新能力索引），以及按模块的 `docs/module-*.md`（core/app/acquisition/processing/tracking/storage/comm/network/gpu/ui）。修改代码前回到对应模块文档的“线程与所有权”“错误路径”“测试入口”确认约束。
