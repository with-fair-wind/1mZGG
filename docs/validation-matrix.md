# 验证覆盖矩阵

更新：2026-09-29。这里区分已经运行的证据、已配置但尚未运行的入口，以及需要外部设备或参考数据的验收。CTest 条目与 GoogleTest 用例不是同一计数口径：部分 Qt 测试按可执行文件整体注册。

| 层级 | 覆盖范围与入口 | 当前证据 | 不覆盖的内容 |
|---|---|---|---|
| 纯核心回归 | 有界通道、异步写入、处理异常、GEO/LEO/SC/Manual、累计历史、目标退休与归档；普通 CTest 与 sanitizer core 目标 | 本机全量回归及远端核心 ASan/TSan 已执行；结果见下表和执行证据 | 科学精度、真实采集和 UI 交互 |
| Qt 集成 | ReplaySession 状态机、回放/主 VM、资源显示与存储 VM、网络协议及生命周期；默认 Qt/OpenCV Debug 配置 | 最新本机应用与测试构建、263/263 CTest 通过（74.84 秒）；包含显示在途批次重置及 UDP 生命周期回归 | 人工视觉检查、GPU 驱动和实际设备行为 |
| Windows 核心 ASan | 公共脚本省略 `--qt-prefix`，12 个目标，项目和 Conan 依赖插桩 | 2026-09-29 真实 runner 107/107 CTest 通过（9.32 秒）；依赖和项目均为 Clang 23.1.2 | 数据竞争、Qt 生命周期和预编译外部库内部 |
| Windows Qt ASan | 公共脚本传入 `--qt-prefix`，核心加回放服务/VM/主 VM/显示 VM、UdpChannel、DataExchange、ImageSender，共 19 个目标 | 2026-09-29 本机强制重建依赖后 119/119（40.39 秒），真实 runner 119/119（14.30 秒）；依赖和项目均为 Clang 23.1.2 | 全量 Qt/UI、OpenCV、预编译 Qt DLL 内部、数据竞争 |
| Linux TSan | 公共脚本 `--sanitizer thread`；Ubuntu 24.04 / GCC 14 核心任务 | 2026-09-29 真实 runner 107/107 CTest 通过（1.86 秒） | Qt 套件未纳入此任务；未执行路径的数据竞争 |
| Windows Release CI | MSVC / Qt 6.8.0 / OpenCV；全量回归及部署目录检查 | 2026-09-29 真实 runner 263/263 CTest 通过（9.65 秒），部署检查及产物上传成功 | 安装包的人工交互验收与真实设备测试 |
| 最新稳定工具链 | 独立 Windows Qt ASan；LLVM/Conan/CMake/Ninja 滚动，Qt 6.8.0 固定，依赖缓存隔离 | 已配置；首轮远端结果待验证，版本与日志随每次运行上传 | 整机所有工具最新版、本机 Scoop 环境完全复刻、自动升级基线 |
| Release 真实 I/O | 生产 GEO、文件 writer、DataExchange 和 UDP 回环，逐字节核验报文与接受请求归档 | 10,000 帧无节流和固定 500 帧/s 场景已运行，指标及拒绝数见资源验证 | 跨机交付、断电持久化、长期满负载 RAW、生产帧率保证 |
| Doxygen | 全项目文档生成；补齐参数、返回值、资源预算、队列与停止契约 | 本轮原 67 条警告已消除，生成零警告；未通过抑制诊断达成 | 注释业务语义的形式化证明或全部文档永不偏移 |
| 真实设备与科学验收 | Sapera、串口命令/应答、断连恢复、CUDA 数值与性能、观测黄金数据 | 尚待设备、标定数据和业务容差 | 不得用合成序列、模拟端或 ASan 通过替代 |

## 执行与证据

- 正常构建与回归：`cmake --build build/clang-cl-debug --parallel 4`，随后 `ctest --test-dir build/clang-cl-debug --output-on-failure --no-tests=error --timeout 90`；Doxygen 用 `cmake --build build/clang-cl-debug --target doxygen`。
- Sanitizer：按 [sanitizer 验证](sanitizer-validation.md) 准备 VS/Conan/Qt 环境，使用 `tools/run_sanitizer.py`；JUnit 与 CTest 日志由脚本/工作流保存。ASan 只检查实际执行且可观测的内存路径，零报告不是不存在所有内存错误的证明。
- 性能：按 [资源验证](resource-validation.md) 运行独立 Release 工具；测量期间避免并行构建/测试，保留输入参数、工具链、report.json 和输出文件。成功写入与容量拒绝必须一起报告。
- 远端：代码提交 `3c7927f` 的 [MSVC Release CI](https://github.com/with-fair-wind/1mZGG/actions/runs/36507360032) 和 [Windows core/Qt ASan、Linux TSan](https://github.com/with-fair-wind/1mZGG/actions/runs/36507360000) 全部通过。Windows 两个 ASan 任务的日志确认 fmt、GTest、spdlog 和项目均由 Clang 23.1.2 编译；Release 部署产物已上传。本地 YAML 解析或推送成功均不能代替这些实际 runner 结果。

后续执行顺序与尚未满足的外部条件见 [实施计划](implementation-plan.md)。本文记录指定验证快照，不将本地日志或工作流配置视为远端 runner 已通过。
