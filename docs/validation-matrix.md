# 验证覆盖矩阵

更新：2026-10-20。这里区分已经运行的证据、已配置但尚未运行的入口，以及需要外部设备或参考数据的验收。CTest 条目与 GoogleTest 用例不是同一计数口径：部分 Qt 测试按可执行文件整体注册。

| 层级 | 覆盖范围与入口 | 当前证据 | 不覆盖的内容 |
|---|---|---|---|
| 纯核心回归 | 有界通道、异步写入、处理异常、GEO/LEO/SC/Manual、累计历史、目标退休与归档；普通 CTest 与 sanitizer core 目标 | 本机全量回归及远端核心 ASan/TSan 已执行；结果见下表和执行证据 | 科学精度、真实采集和 UI 交互 |
| Qt 集成 | ReplaySession、回放/主 VM、资源/存储 VM、网络及窗口生命周期；默认 Qt/OpenCV | 2026-10-20 本机 Debug/PCH 284/284（127.92 秒）、Release/PCH 284/284（40.68 秒）；包含无损存储、完成屏障与真实组合根回放。Linux 专属磁盘满用例不计入 Windows 数量 | 人工视觉检查、GPU 驱动和实际设备行为 |
| Windows 核心 ASan | 公共脚本省略 `--qt-prefix`，当前 14 个目标，项目和 Conan 依赖插桩 | 2026-10-20 远端快照 `b7ee10e` 为 121/121（10.89 秒）；新策略的本机 Qt ASan 同时执行全部核心目标，远端旧计数不作为新策略覆盖 | 数据竞争、Qt 生命周期和预编译外部库内部 |
| Windows Qt ASan | 核心加帧源协调器、回放/主/显示/日志 VM、网络及独立窗口销毁回归，当前共 25 个目标 | 2026-10-20 本机 154/154（41.48 秒），Clang 23.1.3 / Qt 6.11.1，包含组合根回放/关闭；无 ASan 报告。远端快照 `b7ee10e` 为 141/141（12.93 秒，24 个目标） | 全量 Qt/UI、OpenCV、预编译 Qt DLL 内部、数据竞争 |
| Linux TSan | 公共脚本 `--sanitizer thread`；Ubuntu 24.04 / GCC 14 核心任务 | 2026-10-20 真实 runner 快照 `b7ee10e` 为 124/124（1.63 秒），日志确认轨迹、RAW `/dev/full` 和 IMI 索引关闭失败/恢复实际通过；新策略套件另行按提交核对 | Qt 套件未纳入此任务；未执行路径的数据竞争；真实卷耗尽或断电 |
| Windows Release CI | MSVC / Qt 6.8.0 / OpenCV；全量回归及部署检查 | 2026-10-20 真实 runner 快照 `b7ee10e`：272/272（8.32 秒），部署检查及产物上传成功 | 安装包的人工交互验收与真实设备测试 |
| 最新稳定工具链 | 独立 Windows Qt ASan；LLVM/Conan/CMake/Ninja 滚动，Qt 6.8.0 固定，依赖缓存隔离 | 2026-10-20 真实 runner 快照 `b7ee10e`：141/141（14.67 秒，新策略扩展前）；LLVM 23.1.3 / Conan 2.33.0 / CMake 4.4.4 / Ninja 1.13.2，版本与日志产物已上传 | 整机所有工具最新版、本机 Scoop 环境完全复刻、自动升级基线 |
| Release 真实 I/O | 生产 GEO、文件 writer、DataExchange 与逐字节 UDP 回环 | 2026-10-20 复测 1000 帧/s 十分钟，599997 批次零拒绝，轨迹工作峰值 40.38 ms；约 1500/2000 帧/s 各 30 秒及 512/6144 RAW 各一分钟零拒绝。独立核对 5688840 行与 6120 个 RAW 全部像素，长测归档与历史文件逐字节一致；另测无损/实时无节流对照，独立核验 160000 行及 4885 RAW；具体二进制与版本边界见资源验证 | 跨机交付、断电持久化、小时级满负载 RAW、生产帧率保证 |
| 格式与静态分析 | 默认编译数据库的 141 个源文件及 104 个活动头文件；例外范围见 README | `b7ee10e` 的真实 Code quality 门禁通过，含 104 个活动头文件与完整源码。新策略本机执行同样的 104 头文件和 141 源码门禁及全项目格式检查 | 非默认硬件/平台条件编译、第三方/生成文件及未执行路径；不能证明不存在所有缺陷 |
| Doxygen | 全项目文档生成；参数、返回值、资源预算、队列、停止与关闭契约 | 2026-10-20 同步取消/代次、等待者资源与完成屏障契约后重新生成，零警告 | 注释业务语义的形式化证明或全部文档永不偏移 |
| 真实设备与科学验收 | Sapera、串口命令/应答、断连恢复、CUDA 数值与性能、观测黄金数据 | 尚待设备、标定数据和业务容差 | 不得用合成序列、模拟端或 ASan 通过替代 |

## 执行与证据

- 本表维护的远端快照为 `b7ee10e`，四个工作流均于 2026-10-20 成功：[MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/39358897796)、[Code quality](https://github.com/with-fair-wind/1mZGG/actions/runs/39358897787)、[Windows core/Qt ASan 与 Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/39358897591)、[Latest stable toolchain](https://github.com/with-fair-wind/1mZGG/actions/runs/39358897651)。该快照包含窗口生命周期、writer 阶段计时/目录快速路径及轨迹/图像/索引 ENOSPC。新增无损策略及组合根 sanitizer 入口之后，必须检查对应提交的真实 runner，不沿用此快照的测试计数。


- 代码质量：需要 Python 3.9+ 和 Ninja，先完成当前配置的全量构建，再运行 `cmake --build --preset clang-cl-debug --target format-check tidy-check`；单独验证头文件门禁可在 VS/LLVM 环境运行 `python -B -m unittest discover -s tests -p test_include_check.py`。工作流入口为 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/workflows/quality.yml)，其 runner 结果须按具体提交核对，不能由本机通过推断。
- 正常构建与回归：`cmake --build build/clang-cl-debug --parallel 4`，随后 `ctest --test-dir build/clang-cl-debug --output-on-failure --no-tests=error --timeout 90`；Doxygen 用 `cmake --build build/clang-cl-debug --target doxygen`。
- Sanitizer：按 [sanitizer 验证](sanitizer-validation.md) 准备 VS/Conan/Qt 环境，使用 `tools/run_sanitizer.py`；JUnit 与 CTest 日志由脚本/工作流保存。ASan 只检查实际执行且可观测的内存路径，零报告不是不存在所有内存错误的证明。
- 性能：按 [资源验证](resource-validation.md) 运行独立 Release 工具；测量期间避免并行构建/测试，保留输入参数、工具链、report.json 和输出文件。成功写入与容量拒绝必须一起报告。
- 代码提交 `59ed9a0` 的 [Code quality](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748308) 已执行并通过，日志确认 LLVM 23.1.3 / Conan 2.32.0 / CMake 4.4.3 / Ninja 1.13.2；禁用 PCH，扫描当时的 139 个源文件。新入口使后续默认配置增加一个源文件，同一入口会按编译数据库分析它。
- 同一源码的 [MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748284)、[Windows core/Qt ASan 与 Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748167)、[Latest stable toolchain](https://github.com/with-fair-wind/1mZGG/actions/runs/38194748221) 均已通过。上述版本与计数属于该提交的真实 runner 证据，未以本机成功或配置解析替代。
- 固定版本 Qt ASan 首轮中，UDP 用例已通过但进程退出超时；相同源码重跑通过，未取得足以确认该次超时根因的进程堆栈。随后补齐网络测试的 Qt 应用入口与回调状态寿命，本机 Qt ASan 128/128，并用 50 个独立 ASan 进程验证退出；原超时限制保留。若再出现通过后的退出超时，应先保留进程堆栈与完整日志。


后续执行顺序与尚未满足的外部条件见 [实施计划](implementation-plan.md)。本文记录指定验证快照，不将本地日志或工作流配置视为远端 runner 已通过。

Linux `/dev/full` 分别覆盖轨迹、RAW 写入及 IMI 索引关闭失败。异步路径核对失败计数、错误事件及排空；索引路径核对原会话/代次保留与修复后重试。测试只编译于 Linux，由核心 sanitizer 入口选入；Windows 本机不执行，远端快照须按上述具体提交区分。该设备返回真实 ENOSPC，不代表填满实际部署卷或验证断电恢复。
