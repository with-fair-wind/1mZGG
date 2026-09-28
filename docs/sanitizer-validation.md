# Sanitizer 验证指南

## 公共入口

`tools/run_sanitizer.py` 使用独立 Release 目录，默认关闭 Qt；`--qt-prefix` 可增加回放、显示与网络测试。OpenCV/CUDA/Sapera 保持关闭。脚本以同一 sanitizer 编译 Conan 的 GTest/fmt/spdlog，包 ID 包含插桩选项；不会修改用户 profile。构建目录必须位于仓库 build 下。

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

核心套件构建 12 个目标，覆盖有界通道、异步写入队列、处理异常、四种跟踪器及候选/预测/生命周期辅助函数、轨迹存储和结果网络映射。可选 Qt 套件再增加 ReplaySession、回放/主/显示 ViewModel、UdpChannel、DataExchange、ImageSender，共 19 个目标。CTest 每项超时 90 秒、无匹配测试即失败；生成构建目录中的 `sanitizer-results.xml` 和 `Testing/Temporary` 日志。

`.github/workflows/sanitizers.yml` 配置 Windows core/qt ASan 矩阵与 Linux core TSan，在目标分支推送、PR 或手动触发时运行。Windows Qt 固定为 6.8.0 / MSVC 2022，安装 qtcharts、qtserialport；每项保留独立 JUnit 和日志，失败不取消其他矩阵项。普通 MSVC Release CI 继续运行全量回归。

ASan 不检测数据竞争；TSan 也不能代替内存和业务正确性测试。预编译 Qt DLL 内部未插桩；可选 Qt 套件不代表全量 UI、OpenCV、真实相机、CUDA 或生产网络验收。完整边界见 [验证矩阵](validation-matrix.md)。

## 工具链注意事项

- 本机 Windows profile 使用 VS 18/v145，CI profile 使用 VS 17/v143；按当前 clang-cl 版本生成设置。如果 Conan 未收录该版本，先配置用户 `settings_user.yml`，不要静默降级版本号。
- Windows clang-cl 仅支持这里配置的单配置 Release/RelWithDebInfo、x64 ASan；依赖 CRT 与 STL 容器注解必须一致，不能混用普通 Debug 库或通过关闭容器注解规避问题。
- CMake 使用 lld-link，需显式链接 ASan runtime/thunk 和 SEH interceptor；项目保留 `/Zi`、`/DEBUG`、`/OPT:NOICF`。编译器升级后由当前资源目录重新定位运行库，公共脚本采用 fresh 配置，避免缓存旧版本路径。
- 本机 Windows ASan 曾在独立嵌套 catch/rethrow 线程程序中复现 VCRUNTIME 异常。ImageProcessor 用栈展开守卫标记失败，再交由线程顶层捕获并报告，避免重复捕获重抛；未关闭检查或过滤异常路径测试。
- 主工作目录的编译数据库用于日常 clangd；sanitizer 使用独立构建目录，不作为默认开发工具链。

## 最新本机结果

2026-09-28，Windows x64 / clang-cl 23.1.2 / VS 18/v145 / Qt 6.11.1：公共 Qt 入口 **119/119 CTest 通过，41.60 秒**，无 ASan 报告。显示测试内部有 9 个 GoogleTest 用例，覆盖 CPU/RAW 回调期间重置、拉伸重绘取消与新帧继续投递。普通 Debug 全量为 **263/263，74.84 秒**，Doxygen 零警告。

本机证据保存在已忽略目录 `build/architecture-review/display-fix-asan-verified.log`、`display-fix-full-verified.log`；JUnit 在 `build/clang-cl-asan-replay/sanitizer-results.xml`。这些是本地记录，不随源码分发；其他环境应运行上述公共入口生成自己的证据。本机 Qt 版本与 CI 不同，且没有可用 Linux 环境；截至此验证快照，尚无远端 Windows/TSan runner 通过记录。后续以实际工作流结果更新 [验证矩阵](validation-matrix.md)。
