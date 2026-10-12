# 验证覆盖矩阵

更新：2026-10-12。这里区分已经运行的证据、已配置但尚未运行的入口，以及需要外部设备或参考数据的验收。CTest 条目与 GoogleTest 用例不是同一计数口径：部分 Qt 测试按可执行文件整体注册。

| 层级 | 覆盖范围与入口 | 当前证据 | 不覆盖的内容 |
|---|---|---|---|
| 纯核心回归 | 有界通道、异步写入、处理异常、GEO/LEO/SC/Manual、累计历史、目标退休与归档；普通 CTest 与 sanitizer core 目标 | 本机全量回归及远端核心 ASan/TSan 已执行；结果见下表和执行证据 | 科学精度、真实采集和 UI 交互 |
| Qt 集成 | ReplaySession 状态机、回放/主 VM、资源显示与存储 VM、网络协议及生命周期；默认 Qt/OpenCV Debug 配置 | 本机 Debug 266/266（26.22 秒）、Release 无 PCH 266/266（13.99 秒）；包含显示在途批次重置、UDP 生命周期、帧源停止异常、事件引用与 NaN 边界及 Qt 槽元数据回归 | 人工视觉检查、GPU 驱动和实际设备行为 |
| Windows 核心 ASan | 公共脚本省略 `--qt-prefix`，13 个目标，项目和 Conan 依赖插桩 | 2026-10-12 真实 runner 109/109（9.90 秒），依赖与项目均为 Clang 23.1.2 | 数据竞争、Qt 生命周期和预编译外部库内部 |
| Windows Qt ASan | 公共脚本传入 `--qt-prefix`，核心加帧源协调器、回放服务/VM/主 VM/显示 VM/日志 VM、UdpChannel、DataExchange、ImageSender，共 22 个目标 | 2026-10-10 本机 128/128（36.22 秒），依赖与项目均为 Clang 23.1.3；同日远端 128/128（15.43 秒）；退出超时的证据边界见下文 | 全量 Qt/UI、OpenCV、预编译 Qt DLL 内部、数据竞争 |
| Linux TSan | 公共脚本 `--sanitizer thread`；Ubuntu 24.04 / GCC 14 核心任务 | 2026-10-12 真实 runner 109/109（2.04 秒） | Qt 套件未纳入此任务；未执行路径的数据竞争 |
| Windows Release CI | MSVC / Qt 6.8.0 / OpenCV；全量回归及部署目录检查 | 2026-10-12 真实 runner 266/266（8.41 秒），部署检查及产物上传成功 | 安装包的人工交互验收与真实设备测试 |
| 最新稳定工具链 | 独立 Windows Qt ASan；LLVM/Conan/CMake/Ninja 滚动，Qt 6.8.0 固定，依赖缓存隔离 | 2026-10-12 真实 runner 128/128（16.16 秒）；LLVM 23.1.3 / Conan 2.33.0 / CMake 4.4.4 / Ninja 1.13.2，版本与日志产物上传成功 | 整机所有工具最新版、本机 Scoop 环境完全复刻、自动升级基线 |
| Release 真实 I/O | 生产 GEO、文件 writer、DataExchange 和 UDP 回环，逐字节核验报文与接受请求归档 | 10,000 帧无节流和固定 500 帧/s 场景已运行，指标及拒绝数见资源验证 | 跨机交付、断电持久化、长期满负载 RAW、生产帧率保证 |
| 格式与静态分析 | `format-check` 扫描 Git 跟踪的项目 C/C++ 文件；`tidy-check` 先将 103 个活动头文件作为主文件检查冗余 include，再扫描默认配置中的 140 个源文件 | 既有固定 LLVM 23.1.3 门禁已通过；本轮新增独立头文件检查，本机格式与完整 clang-tidy 通过，Debug/PCH、Release/无 PCH 及 Release/PCH 的头文件检查均 103/103 无诊断，三种配置回归各 266/266；真实 Git/Ninja 正反例已验证 clang-cl/clang++ 的 PCH 参数处理及构建依赖不足时提前退出。远端新增覆盖须以本轮提交的工作流结果为准 | 非默认硬件配置、第三方/生成文件、所有潜在缺陷的证明；宏参数与有原因的局部例外见 README |
| Doxygen | 全项目文档生成；补齐参数、返回值、资源预算、队列与停止契约 | 本轮原 67 条警告已消除，生成零警告；未通过抑制诊断达成 | 注释业务语义的形式化证明或全部文档永不偏移 |
| 真实设备与科学验收 | Sapera、串口命令/应答、断连恢复、CUDA 数值与性能、观测黄金数据 | 尚待设备、标定数据和业务容差 | 不得用合成序列、模拟端或 ASan 通过替代 |

## 执行与证据

- 代码质量：需要 Python 3.9+ 和 Ninja，先完成当前配置的全量构建，再运行 `cmake --build --preset clang-cl-debug --target format-check tidy-check`；单独验证头文件门禁可在 VS/LLVM 环境运行 `python -B -m unittest discover -s tests -p test_include_check.py`。工作流入口为 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/workflows/quality.yml)，其 runner 结果须按具体提交核对，不能由本机通过推断。
- 正常构建与回归：`cmake --build build/clang-cl-debug --parallel 4`，随后 `ctest --test-dir build/clang-cl-debug --output-on-failure --no-tests=error --timeout 90`；Doxygen 用 `cmake --build build/clang-cl-debug --target doxygen`。
- Sanitizer：按 [sanitizer 验证](sanitizer-validation.md) 准备 VS/Conan/Qt 环境，使用 `tools/run_sanitizer.py`；JUnit 与 CTest 日志由脚本/工作流保存。ASan 只检查实际执行且可观测的内存路径，零报告不是不存在所有内存错误的证明。
- 性能：按 [资源验证](resource-validation.md) 运行独立 Release 工具；测量期间避免并行构建/测试，保留输入参数、工具链、report.json 和输出文件。成功写入与容量拒绝必须一起报告。
- 代码提交 `59ed9a0` 的 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748308) 已执行并通过，日志确认 LLVM 23.1.3 / Conan 2.32.0 / CMake 4.4.3 / Ninja 1.13.2；禁用 PCH，扫描当时的 139 个源文件。新入口使后续默认配置增加一个源文件，同一入口会按编译数据库分析它。
- 同一源码的 [MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748284)、[Windows core/Qt ASan 与 Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748167)、[Latest stable toolchain](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748221) 均已通过。上述版本与计数属于该提交的真实 runner 证据，未以本机成功或配置解析替代。
- 固定版本 Qt ASan 首轮中，UDP 用例已通过但进程退出超时；相同源码重跑通过，未取得足以确认该次超时根因的进程堆栈。随后补齐网络测试的 Qt 应用入口与回调状态寿命，本机 Qt ASan 128/128，并用 50 个独立 ASan 进程验证退出；原超时限制保留。若再出现通过后的退出超时，应先保留进程堆栈与完整日志。


后续执行顺序与尚未满足的外部条件见 [实施计划](implementation-plan.md)。本文记录指定验证快照，不将本地日志或工作流配置视为远端 runner 已通过。
