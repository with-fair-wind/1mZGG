# 验证覆盖矩阵

更新：2026-10-20。这里区分已经运行的证据、已配置但尚未运行的入口，以及需要外部设备或参考数据的验收。CTest 条目与 GoogleTest 用例不是同一计数口径：部分 Qt 测试按可执行文件整体注册。

| 层级 | 覆盖范围与入口 | 当前证据 | 不覆盖的内容 |
|---|---|---|---|
| 纯核心回归 | 有界通道、异步写入、处理异常、GEO/LEO/SC/Manual、累计历史、目标退休与归档；普通 CTest 与 sanitizer core 目标 | 本机全量回归及远端核心 ASan/TSan 已执行；结果见下表和执行证据 | 科学精度、真实采集和 UI 交互 |
| Qt 集成 | ReplaySession 状态机、回放/主 VM、资源显示与存储 VM、网络协议及生命周期；默认 Qt/OpenCV Debug 配置 | 2026-10-20 本机 Windows Debug/PCH 272/272（40.77 秒）、Release/PCH 272/272（26.32 秒）；包含窗口销毁、轨迹计时默认关闭、异常展开、跨启停累计及目录移除/恢复。Linux 专属 /dev/full 未在本机执行；既有 Release/无 PCH 结果仍为历史快照 | 人工视觉检查、GPU 驱动和实际设备行为 |
| Windows 核心 ASan | 公共脚本省略 `--qt-prefix`，13 个目标，项目和 Conan 依赖插桩 | 2026-10-12 真实 runner 109/109（9.90 秒），依赖与项目均为 Clang 23.1.2 | 数据竞争、Qt 生命周期和预编译外部库内部 |
| Windows Qt ASan | 公共脚本传入 `--qt-prefix`，核心加帧源协调器、回放服务/VM/主 VM/显示 VM/日志 VM、UdpChannel、DataExchange、ImageSender 与独立窗口销毁回归，共 23 个目标 | 2026-10-20 本机 134/134（51.16 秒），依赖与项目均为 Clang 23.1.3，Qt 6.11.1；包含独立窗口销毁、轨迹计时/恢复与异常展开，无 ASan 报告。远端当前已提交基线通过，新增覆盖尚待提交后的 runner 验证 | 全量 Qt/UI、OpenCV、预编译 Qt DLL 内部、数据竞争 |
| Linux TSan | 公共脚本 `--sanitizer thread`；Ubuntu 24.04 / GCC 14 核心任务 | 2026-10-12 真实 runner 109/109（2.04 秒） | Qt 套件未纳入此任务；未执行路径的数据竞争 |
| Windows Release CI | MSVC / Qt 6.8.0 / OpenCV；全量回归及部署目录检查 | 2026-10-12 真实 runner 266/266（8.41 秒），部署检查及产物上传成功 | 安装包的人工交互验收与真实设备测试 |
| 最新稳定工具链 | 独立 Windows Qt ASan；LLVM/Conan/CMake/Ninja 滚动，Qt 6.8.0 固定，依赖缓存隔离 | 2026-10-12 真实 runner 128/128（16.16 秒）；LLVM 23.1.3 / Conan 2.33.0 / CMake 4.4.4 / Ninja 1.13.2，版本与日志产物上传成功 | 整机所有工具最新版、本机 Scoop 环境完全复刻、自动升级基线 |
| Release 真实 I/O | 生产 GEO、文件 writer、DataExchange 和 UDP 回环，逐字节核验报文与接受请求归档 | 2026-10-19 增加 100～1000 帧/s 扫描及 1000 帧/s 的 60 万帧、10 分钟场景，599,997 个轨迹批次零拒绝；512 RAW 全程 3000 帧，6144 突发接受 4/16 帧。实际归档字段和全部 RAW 像素独立核验通过；writer 曾有 410 ms 长尾；2026-10-20 增加阶段计时和目录快速路径，三对交替负载单批已测阶段成本中位数降低约 37%，固定负载归档逐字节一致，详见资源验证 | 跨机交付、断电持久化、长期满负载 RAW、生产帧率保证 |
| 格式与静态分析 | 默认编译数据库的 141 个源文件及 104 个活动头文件；宏与局部例外见 README | 2026-10-20 格式通过；103 个已跟踪头文件门禁通过，尚未跟踪的新计时头文件用实际 Ninja 消费者参数、移除 PCH 后独立完整分析通过。修复枚举末尾逗号、冗余初始化和基准复杂度诊断后，按同一共享规则及 warning 即失败参数，以 4 个进程复核全部 141 个源码，141/141 通过。新文件另经格式检查。远端仅确认已提交基线，本轮尚未提交 | 非默认硬件/其他平台条件编译、第三方/生成文件及未执行路径；不能证明不存在所有缺陷 |
| Doxygen | 全项目文档生成；补齐参数、返回值、资源预算、队列与停止契约 | 既有 67 条警告已消除；2026-10-20 补充 writer 阶段计时、关闭检查和恢复契约后重新生成，零警告 | 注释业务语义的形式化证明或全部文档永不偏移 |
| 真实设备与科学验收 | Sapera、串口命令/应答、断连恢复、CUDA 数值与性能、观测黄金数据 | 尚待设备、标定数据和业务容差 | 不得用合成序列、模拟端或 ASan 通过替代 |

## 执行与证据

- 当前已提交基线 `729908f` 的四个工作流均在 2026-10-15 成功： [MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/38702277874)、[Code quality](https://github.com/with-fair-wind/1mZGG/actions/runs/38702277880)、[Windows core/Qt ASan 与 Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/38702277866)、[Latest stable toolchain](https://github.com/with-fair-wind/1mZGG/actions/runs/38702277930)。这确认独立头文件检查与许可产物校验已经过真实 runner；不代表本轮未提交的变更已由远端验收。

- 代码质量：需要 Python 3.9+ 和 Ninja，先完成当前配置的全量构建，再运行 `cmake --build --preset clang-cl-debug --target format-check tidy-check`；单独验证头文件门禁可在 VS/LLVM 环境运行 `python -B -m unittest discover -s tests -p test_include_check.py`。工作流入口为 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/workflows/quality.yml)，其 runner 结果须按具体提交核对，不能由本机通过推断。
- 正常构建与回归：`cmake --build build/clang-cl-debug --parallel 4`，随后 `ctest --test-dir build/clang-cl-debug --output-on-failure --no-tests=error --timeout 90`；Doxygen 用 `cmake --build build/clang-cl-debug --target doxygen`。
- Sanitizer：按 [sanitizer 验证](sanitizer-validation.md) 准备 VS/Conan/Qt 环境，使用 `tools/run_sanitizer.py`；JUnit 与 CTest 日志由脚本/工作流保存。ASan 只检查实际执行且可观测的内存路径，零报告不是不存在所有内存错误的证明。
- 性能：按 [资源验证](resource-validation.md) 运行独立 Release 工具；测量期间避免并行构建/测试，保留输入参数、工具链、report.json 和输出文件。成功写入与容量拒绝必须一起报告。
- 代码提交 `59ed9a0` 的 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748308) 已执行并通过，日志确认 LLVM 23.1.3 / Conan 2.32.0 / CMake 4.4.3 / Ninja 1.13.2；禁用 PCH，扫描当时的 139 个源文件。新入口使后续默认配置增加一个源文件，同一入口会按编译数据库分析它。
- 同一源码的 [MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748284)、[Windows core/Qt ASan 与 Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748167)、[Latest stable toolchain](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748221) 均已通过。上述版本与计数属于该提交的真实 runner 证据，未以本机成功或配置解析替代。
- 固定版本 Qt ASan 首轮中，UDP 用例已通过但进程退出超时；相同源码重跑通过，未取得足以确认该次超时根因的进程堆栈。随后补齐网络测试的 Qt 应用入口与回调状态寿命，本机 Qt ASan 128/128，并用 50 个独立 ASan 进程验证退出；原超时限制保留。若再出现通过后的退出超时，应先保留进程堆栈与完整日志。


后续执行顺序与尚未满足的外部条件见 [实施计划](implementation-plan.md)。本文记录指定验证快照，不将本地日志或工作流配置视为远端 runner 已通过。

Linux `/dev/full` 回归验证缓冲写入可能直到关闭时才返回 ENOSPC，并核对失败计数与错误事件。测试仅编译于 Linux，会由既有核心 sanitizer 入口按测试名前缀选入；本轮在 Windows 执行验证，Linux 用例不计为已运行或远端通过。
