# Sanitizer 验证指南

## 公共入口

`tools/run_sanitizer.py` 使用独立 Release 目录，默认关闭 Qt；`--qt-prefix` 可增加回放、显示与网络测试。OpenCV/CUDA/Sapera 保持关闭。脚本以同一 sanitizer 编译 Conan 的 GTest/fmt/spdlog，包 ID 包含插桩选项；Windows 先解析 clang-cl 的绝对路径，再统一传给 Conan 依赖和项目，保证编译器与 ASan 运行库一致，不受 vcvars 重排 PATH 的影响。不会修改用户 profile；构建目录必须位于仓库 build 下。

Windows 在 x64 Visual Studio 开发者终端运行，确保 clang-cl、Conan、CMake、Ninja 在 PATH；准备兼容的 host profile 和 Conan default build profile：

```powershell
# 核心套件
python tools/run_sanitizer.py --sanitizer address --build-dir build/clang-cl-asan-release --host-profile profiles/conan/windows-clang-cl --offline
# 增加 Qt 套件；替换为本机 Qt 安装前缀
python tools/run_sanitizer.py --sanitizer address --build-dir build/clang-cl-asan-replay --host-profile profiles/conan/windows-clang-cl --qt-prefix D:/Qt/6.11.1/msvc2022_64 --offline
```

`--offline` 要求依赖源码/包已在缓存，否则去掉此选项以获取依赖。Qt 前缀必须包含 `lib/cmake/Qt6/Qt6Config.cmake`；脚本将 Qt bin 与当前编译器的 ASan DLL 目录加入 PATH，并默认使用 offscreen Qt 平台。

Linux 使用支持 C++23 的 GCC/Clang profile；CI 配置为 GCC 14：

```bash
CC=gcc-14 CXX=g++-14 conan profile detect --name default
python tools/run_sanitizer.py --sanitizer thread --build-dir build/ci-thread --host-profile default
```

## 覆盖与持续集成

核心套件构建 14 个目标，覆盖事件模板的引用类别和多播值复制、有界通道、异步写入队列、处理异常、四种跟踪器及候选/预测/生命周期辅助函数、图像/轨迹存储和结果网络映射。可选 Qt 套件再增加 FrameSourceCoordinator、ReplaySession、回放/主/显示/日志 ViewModel（含槽元数据）、UdpChannel、DataExchange、ImageSender，并包含独立的窗口销毁回归，共 24 个目标。CTest 每项超时 90 秒、无匹配测试即失败；生成构建目录中的 `sanitizer-results.xml` 和 `Testing/Temporary` 日志。

`.github/workflows/sanitizers.yml` 配置 Windows core/qt ASan 矩阵与 Linux core TSan，在目标分支推送、PR 或手动触发时运行。Windows 固定 LLVM 23.1.2（官方 MSI 校验 SHA-256 后解包），Qt 固定为 6.8.0 / MSVC 2022，安装 qtcharts、qtserialport；每项保留独立 JUnit 和日志，失败不取消其他矩阵项。普通 MSVC Release CI 继续运行全量回归。

ASan 不检测数据竞争；TSan 也不能代替内存和业务正确性测试。预编译 Qt DLL 内部未插桩；窗口回归仅验证 ViewModel 晚于页面销毁时的直接/排队更新与拉伸定时器，不使用原生 Dock 或 GPU 断言。可选 Qt 套件不代表全量 UI、OpenCV、真实相机、CUDA 或生产网络验收。完整边界见 [验证矩阵](validation-matrix.md)。

UDP、DataExchange、ImageSender 测试使用共同的 `QCoreApplication` 入口，应用对象先于 socket 等 Qt 对象构造，并在全部测试和工作线程结束后销毁。回调引用的 promise 也必须晚于通道销毁，涵盖断言提前退出路径；用独立进程重复运行才能验证进程清理，GoogleTest 输出通过本身不证明进程已退出。

## 最新稳定工具链兼容性

独立工作流 `.github/workflows/toolchain-latest.yml` 在同样的推送、PR 和手动入口运行 Windows Qt ASan。每次解析 LLVM 官方 latest 稳定发布，校验 Windows x64 MSI 的 SHA-256 与实际编译器版本；Conan、CMake、Ninja 由 pip 安装当前 Python 3.12 可用的最新稳定版。Qt 暂时固定 6.8.0，runner 固定为 windows-2022；这不是整台环境所有组件都使用最新版，也不自动复制本机 Scoop 环境。

每次任务使用全新的 `.conan-latest`，不恢复或保存依赖缓存，并传入 `--rebuild-deps` 强制源码重建依赖，避免编译器补丁版本变化后复用旧插桩包或下载远端旧二进制。编译器主版本只扩展到该任务的 Conan `settings_user.yml`。依赖版本仍遵守 `conanfile.py`；解析后的 recipe/package revision 和插桩参数保存在构建日志中。

产物 `windows-latest-toolchain-qt-asan` 保留 14 天，包含 LLVM 下载元数据、pip 安装报告、实际工具路径与版本、MSVC/SDK、Qt、Conan profiles、构建日志、CMake 配置和 JUnit。`tools/record_ci_toolchain.py` 在激活后的 VS 环境中采集白名单字段，不导出整个环境；这些记录用于定位某次运行，不等于锁定未来构建。

滚动任务保持真实失败状态，不通过 `continue-on-error`、跳过异常测试或关闭 ASan 掩盖失败。初期建议不将它加入分支必需检查；GitHub 分支保护需单独配置，此工作流不修改保护规则。新版本通过兼容性验证及现有基线全套回归后，再以明确的版本更新提交提升基线。现有基线的 LLVM/Conan/Qt 有固定版本，但 CMake 范围、Ninja 和 runner 镜像仍会更新，不能视为完全锁定环境。

本机 Scoop 升级后，应重新配置独立构建目录、核对实际编译器与 ASan runtime；若复用了旧 Conan home 且编译器补丁、MSVC 或 SDK 已变化，在本页公共命令上增加 `--rebuild-deps` 重建相关依赖，不能仅凭相同 Clang 主版本推断二进制兼容。优先重跑 Qt ASan，再提交业务变更。

## 工具链注意事项

- 本机 Windows profile 使用 VS 18/v145；CI 使用 VS 2022 的 MSVC 14.4x，Conan 的 clang profile 需设 `compiler.runtime_version=v144` 才会激活 14.4x（微软的平台工具集名称仍为 v143）。clang-cl 版本按实际检测生成；如果 Conan 未收录该版本，先配置用户 `settings_user.yml`，不要静默降级版本号。
- CI 的 Conan 2.29.0 尚未列出 clang 23，因此仅在工作流专用 `CONAN_HOME` 写入 `settings_user.yml` 扩展版本；调用 vcvars 后重新把固定 LLVM 放到 PATH 首位，避免重新选中 VS 自带的 Clang 19。后者缺少 [LLVM Windows ASan catch 参数修复](https://github.com/llvm/llvm-project/pull/159618)，会在正常捕获异常时崩溃；不通过过滤异常测试或关闭插桩处理此问题。
- Windows clang-cl 仅支持这里配置的单配置 Release/RelWithDebInfo、x64 ASan；依赖 CRT 与 STL 容器注解必须一致，不能混用普通 Debug 库或通过关闭容器注解规避问题。
- CMake 使用 lld-link，需显式链接 ASan runtime/thunk 和 SEH interceptor；项目保留 `/Zi`、`/DEBUG`、`/OPT:NOICF`。Conan 的链接 flags 会嵌入 CMake 字符串，库路径的引号需转义，才能支持 `Program Files` 等含空格目录。编译器升级后由当前资源目录重新定位运行库，公共脚本采用 fresh 配置，避免缓存旧版本路径。
- 本机 Windows ASan 曾在独立嵌套 catch/rethrow 线程程序中复现 VCRUNTIME 异常。ImageProcessor 用栈展开守卫标记失败，再交由线程顶层捕获并报告，避免重复捕获重抛；未关闭检查或过滤异常路径测试。
- 主工作目录的编译数据库用于日常 clangd；sanitizer 使用独立构建目录，不作为默认开发工具链。

## 最新本机结果

2026-10-20，Windows x64 / clang-cl 23.1.3 / VS 18/v145 / Qt 6.11.1：项目及 Conan 插桩依赖使用同一编译器，公共 Qt 入口 **141/141 CTest 通过，36.12 秒**，无 ASan 报告。覆盖独立窗口生命周期及新增 writer 默认关闭、失败恢复/跨启停累计、目录移除恢复与计时异常展开回归，并加入图像后端全部 7 个 Windows 用例及失败恢复。普通 Qt/OpenCV Debug/PCH **272/272，41.74 秒**，Release/PCH **272/272，26.85 秒**，Doxygen 零警告。

提交 `2a56908` 的真实 Linux TSan 已通过 115/115，日志确认轨迹 `/dev/full` 执行成功；同一提交的 Windows core/Qt ASan 分别为 114/114 与 134/134。随后图像存储加入核心入口，另有 RAW 关闭失败和索引关闭失败/恢复两个 Linux 用例；新套件不沿用旧计数。Windows 本机不执行这些平台用例，各 runner 的具体提交与覆盖边界见验证矩阵。

新增窗口目标在普通 Release 初次运行时因部署目录缺少 offscreen 插件而超时；日志明确报告无法初始化平台插件。测试现从实际链接 SDK 的插件目录加载，原超时不扩大，修正后普通与 ASan 独立进程均通过。此环境问题与此前窗口销毁后实际访问已释放控件的问题分别保留回归和证据。

JUnit 位于 `build/clang-cl-asan-replay/sanitizer-results.xml`，CTest 日志位于相同目录的 `Testing/Temporary`；这些是被忽略的本机产物，其他环境应运行公共入口生成自己的证据。当前核心入口包含 14 个目标，Qt 入口共 24 个目标；历史 runner 的测试数量不能当作扩展后套件的结果。本机 Qt 版本与 CI 不同，远端证据及配置边界见 [验证矩阵](validation-matrix.md)。
