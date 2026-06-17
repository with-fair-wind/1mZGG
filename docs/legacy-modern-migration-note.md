# DSS_QT 旧新架构迁移笔记（一页版）

## 目录锚点 + 快速跳转索引

- [1) 入口层对照：单体启动 -> 组合根启动](#sec-1-entry)
- [2) 事件系统对照：历史总线 -> 命名空间化现代总线](#sec-2-event-bus)
- [3) 跟踪算法对照：`TrackAlgo.cpp` 大单体 -> 策略切分](#sec-3-tracking-split)
- [4) 关键接口抽象（迁移主线）](#sec-4-interfaces)
- [5) 模块拆分映射（旧职责 -> 新 target）](#sec-5-targets)
- [6) 本轮已验证测试覆盖（对应迁移链路）](#sec-6-tests)
- [7) 完整阅读清单（可勾选）](#sec-7-checklist)
  - [A. 全局认知（先建地图，不钻细节）](#sec-7-a)
  - [B. 启动与装配主链（项目从哪启动）](#sec-7-b)
  - [C. UI -> ViewModel -> 事件回 UI（第一条业务闭环）](#sec-7-c)
  - [D. 回放与处理管线（第二条业务闭环）](#sec-7-d)
  - [E. 跟踪层（Manual/GEO/LEO/SC）](#sec-7-e)
  - [F. 通信、网络、存储（第三条业务闭环）](#sec-7-f)
  - [G. 旧新对照（最后做，不提前）](#sec-7-g)
  - [H. 测试驱动阅读（每读完一块就验证）](#sec-7-h)
  - [I. 每完成一组模块，必须产出（防止“看过=会了”）](#sec-7-i)

---

<a id="sec-1-entry"></a>

## 1) 入口层对照：单体启动 -> 组合根启动

- 旧版入口在 `oldsrc/main.cpp`：直接在 `main()` 内实例化大量对象（`Comm*`、`Grabber`、`ImageProcessor`、`UI_*`），并手写大量 `QObject::connect(...)`。
- 新版入口在 `src/main.cpp`：只负责 Qt 生命周期、配置加载、`ApplicationContext` 初始化与 `MainWindow` 展示。
- 关键变化：服务装配由 `src/app/communication_services.cpp` 集中完成，通过 `ServiceRegistry` 注册，不再在 `main()` 中散落管理。

结论：启动复杂度从“单函数集中耦合”变为“组合根集中装配 + UI/业务分离”。

<a id="sec-2-event-bus"></a>

## 2) 事件系统对照：历史总线 -> 命名空间化现代总线

- 旧版事件总线在 `oldsrc/eventBus17.hpp`，命名空间为 `evt`。
- 新版在 `include/dss/core/event_bus.h`，迁移到 `Dss::Evt`，并补充了 Qt 兼容处理（对 `emit` 宏做防冲突保护）。
- 新版保留了 COW 快照、ScopedConnection、锁策略（`NoLock` / `SharedMutexLock`）等能力，同时接口命名更统一（如 `forEachSafe`、`ensureChannel`）。

结论：事件机制能力延续，但边界更清晰，且可直接服务于 Qt + 非 Qt 混合工程。

<a id="sec-3-tracking-split"></a>

## 3) 跟踪算法对照：`TrackAlgo.cpp` 大单体 -> 策略切分

- 旧版 `oldsrc/TrackAlgo.cpp` 集中承载 GEO/LEO/SC/Manual 及多类 `Assoc/Track/Verify` 流程，维护和测试成本高。
- 新版拆分为：
  - `src/tracking/geo_tracker.cpp`
  - `src/tracking/leo_tracker.cpp`
  - `src/tracking/sc_tracker.cpp`
  - `src/tracking/manual_tracker.cpp`
  - `src/tracking/track_manager.cpp`（策略工厂与调度）
- `TrackManager` 通过 `makeTrackingStrategy(...)` 根据 `TrackMode` 选择实现，避免旧版大文件中的分支级耦合。

结论：算法迁移采用“策略化 + 分文件 + 可测切片”，可持续迭代明显优于旧版单体模式。

<a id="sec-4-interfaces"></a>

## 4) 关键接口抽象（迁移主线）

- 采集：`IFrameSource`（回放/硬件采集统一入口）
- 处理：`IProcessingStrategy`（None/OpenCV/CUDA 等后端可插拔）
- 跟踪：`ITrackingStrategy`（GEO/LEO/SC/Manual 解耦）
- 串口：`ISerialChannel` + 命令窄接口（曝光/伺服/主控）
- 网络：`INetworkChannel`（图像发送、心跳、诊断、数据交换）
- 存储：`IStorageBackend`（图像/轨迹异步写入）

结论：旧版“对象直接互连”被替换为“接口 + 注册表 + 事件总线”三件套。

<a id="sec-5-targets"></a>

## 5) 模块拆分映射（旧职责 -> 新 target）

- Core 基础能力 -> `dss_core`
- 跟踪算法 -> `dss_tracking`
- 图像处理 -> `dss_processing` / `dss_processing_opencv`
- 串口通信 -> `dss_comm_qt`
- 网络通信 -> `dss_network_qt`
- 应用装配 -> `dss_app`
- UI 层 -> `dss_ui_qt`
- 可执行程序 -> `DSS_QT`

结论：由“源码目录组织”升级为“CMake target 组织”，依赖方向显式可控。

<a id="sec-6-tests"></a>

## 6) 本轮已验证测试覆盖（对应迁移链路）

- 回放到显示：
  - `test_view_model_replay`（通过）
- 处理与事件：
  - `ImageProcessor.PublishesDisplayFramePayload`（通过）
  - `ImageProcessor.PublishesManualTrackResultsWithoutProcessingBackend`（通过）
  - `ImageProcessor.PassesValidatedTargetBlobsToTrackingWithoutProcessingBackend`（通过）
  - `OpenCvProcessingStrategyTest.ComputesStatsAndExtractsBrightBlob`（通过）
- 跟踪/网络/存储：
  - `ManualTracker.*`（3 项通过）
  - `DataExchange.*`（2 项通过）
  - `TrackDataStorageBackend.*`（2 项通过）
  - 以及关联 `DataExchangeProtocol.*`、`TrackResultDataExchangeBridge.*` 用例（通过）

结论：迁移路径已具备“阅读 -> 回溯 -> 测试佐证”的闭环，不再依赖仅凭源码猜测行为。

<a id="sec-7-checklist"></a>

## 7) 完整阅读清单（可勾选）

使用建议：从上到下推进，不按时间切分，按“理解闭环”切分。  
主线始终是：`UI动作 -> ViewModel -> ServiceRegistry -> 处理/跟踪 -> 事件回UI`。

<a id="sec-7-a"></a>

### A. 全局认知（先建地图，不钻细节）

- [ ] 读 `README.md`，先回答：项目目标是什么、当前完成到哪、哪些模块是可选能力（OpenCV/CUDA/Sapera）。
- [ ] 读 `docs/overview.md`，确认模块分层和依赖方向，记住“Qt 只在边界”这个原则。
- [ ] 读 `docs/migration-status.md`，明确哪些功能“已完成/部分完成/未开始”。
- [ ] 读 `docs/legacy-modern-migration-note.md`，先建立旧新映射思路，避免一开始迷失在 `oldsrc`。
- [ ] 浏览 `CMakeLists.txt`，先看 target 依赖，不看每一行细节。
- [ ] 浏览 `CMakePresets.json` 和 `conanfile.py`，知道本地如何构建与测试。

<a id="sec-7-b"></a>

### B. 启动与装配主链（项目从哪启动）

- [ ] 读 `src/main.cpp`，梳理启动顺序：`QApplication`、`ApplicationContext`、`loadConfig`、`registerCommunicationServices`、`ViewModel`、`MainWindow`。
- [ ] 读 `include/dss/app/application_context.h` 与 `src/app/application_context.cpp`，明确组合根职责。
- [ ] 精读 `src/app/communication_services.cpp`，把所有注册 key 整理成表（如 `replay_source`、`image_processor`、`image_storage`、`data_exchange`）。
- [ ] 读 `include/dss/core/service_registry.h`，理解服务按“接口 + 名称”查找的机制。
- [ ] 读 `include/dss/core/config.h`、`include/dss/core/config_types.h`、`src/core/config.cpp`，理解配置对象如何影响服务参数。
- [ ] 读 `config/SystemInit.json`，把配置字段和代码结构一一对应。

<a id="sec-7-c"></a>

### C. UI -> ViewModel -> 事件回 UI（第一条业务闭环）

- [ ] 读 `src/ui/main_window.cpp`，只抓 `connect(...)`：按钮触发了哪些 `ViewModel` 方法。
- [ ] 读 `include/dss/ui/view_model.h`，先看公开命令面，不先看实现细枝末节。
- [ ] 读 `src/ui/view_model.cpp`，先精读 `selectReplayFiles`、`startGrab`、`stopGrab`、`stepReplayForward`。
- [ ] 继续读 `src/ui/view_model.cpp` 的 `setupSubscriptions`，确认订阅了哪些核心事件。
- [ ] 精读 `onDisplayRefresh`、`onProcessingComplete`、`onTrackResult`，理解 UI 状态如何被后端事件驱动。
- [ ] 读 `include/dss/ui/app_event.h` 与 `src/ui/app_event.cpp`，分清 `AppEvent`（UI 层）和 `BasicMessageBus`（后端层）边界。
- [ ] 读 `include/dss/ui/image_display.h` 与 `src/ui/image_display.cpp`，看 `displayImageReady` 最终如何落到控件绘制。

<a id="sec-7-d"></a>

### D. 回放与处理管线（第二条业务闭环）

- [ ] 读 `include/dss/acquisition/i_frame_source.h`，明确帧源抽象接口。
- [ ] 读 `include/dss/acquisition/image_sequence_frame_source.h` 与 `src/acquisition/image_sequence_frame_source.cpp`，搞清 `init/start/stepForward/callback`。
- [ ] 读 `include/dss/processing/frame_packet.h`，把一帧在系统中的数据形态记清楚。
- [ ] 读 `include/dss/processing/bounded_channel.h`，理解处理线程为何不会无限积压。
- [ ] 读 `include/dss/processing/i_processing_strategy.h`，理解策略可插拔约束。
- [ ] 读 `include/dss/processing/processing_pipeline.h` 与 `src/processing/processing_pipeline.cpp`，理解策略挂载点。
- [ ] 精读 `include/dss/processing/image_processor.h` 与 `src/processing/image_processor.cpp`，看 `workerLoop` 发出的事件顺序。
- [ ] 读 `include/dss/processing/opencv_processing_strategy.h` 与 `src/processing/opencv_processing_strategy.cpp`，理解默认参考后端做了哪些计算。
- [ ] 读 `include/dss/core/events.h`，把 `DisplayRefreshEvent`、`ProcessingCompleteEvent`、`TrackResultEvent`、`ImageSendEvent` 作为核心词汇。

<a id="sec-7-e"></a>

### E. 跟踪层（Manual/GEO/LEO/SC）

- [ ] 读 `include/dss/tracking/i_tracking_strategy.h`，理解所有跟踪策略统一入口。
- [ ] 读 `src/tracking/track_manager.cpp`，确认策略创建和切换逻辑。
- [ ] 精读 `src/tracking/manual_tracker.cpp`，先掌握最小可运行跟踪逻辑。
- [ ] 再读 `src/tracking/geo_tracker.cpp`、`src/tracking/leo_tracker.cpp`、`src/tracking/sc_tracker.cpp`，先看 `track(...)` 主流程，再看细节 helper。
- [ ] 读 `include/dss/tracking/candidate_utils.h`、`include/dss/tracking/prediction_utils.h`、`include/dss/tracking/lifecycle_utils.h`，理解公共规则抽取方式。
- [ ] 读 `include/dss/core/result_packet_utils.h`，理解目标结果如何归一化为通信可消费包。

<a id="sec-7-f"></a>

### F. 通信、网络、存储（第三条业务闭环）

- [ ] 读 `include/dss/comm/i_serial_channel.h`，理解串口通道抽象。
- [ ] 精读 `include/dss/comm/serial_protocol_codec.h`，先看帧长、校验、编码/解码入口，再看字段细节。
- [ ] 读 `src/comm/display_channel.cpp`、`src/comm/exposure_channel.cpp`、`src/comm/master_control_channel.cpp`、`src/comm/servo_channel.cpp`，看四路串口职责分配。
- [ ] 读 `include/dss/network/i_network_channel.h`，理解网络通道抽象。
- [ ] 精读 `include/dss/network/data_exchange_protocol.h`，理解 `ResultPacket -> GXTC/GDCL` 映射。
- [ ] 读 `src/network/data_exchange.cpp`、`src/network/image_sender.cpp`、`src/network/heartbeat.cpp`、`src/network/error_diagnostics.cpp`、`src/network/atmos_receiver.cpp`，掌握网络能力拼图。
- [ ] 精读 `src/app/track_result_data_exchange_bridge.cpp`，看跟踪结果如何桥接为网络发送。
- [ ] 读 `include/dss/storage/i_storage_backend.h`，理解存储后端抽象。
- [ ] 读 `include/dss/storage/local_image_storage_backend.h` 与 `include/dss/storage/track_data_storage_backend.h`，理解异步写盘队列模型。

<a id="sec-7-g"></a>

### G. 旧新对照（最后做，不提前）

- [ ] 对照 `oldsrc/main.cpp` 与 `src/main.cpp`，体会“main 里全实例化”到“组合根装配”的迁移。
- [ ] 对照 `oldsrc/eventBus17.hpp` 与 `include/dss/core/event_bus.h`，看命名空间、接口和 Qt 兼容细节变化。
- [ ] 对照 `oldsrc/TrackAlgo.cpp` 与 `src/tracking/geo_tracker.cpp`、`src/tracking/leo_tracker.cpp`、`src/tracking/sc_tracker.cpp`、`src/tracking/manual_tracker.cpp`，理解大单体拆分为策略模块的原则。
- [ ] 对照 `oldsrc/ImageReplayer.*` 与 `src/acquisition/image_sequence_frame_source.cpp`，看回放链路如何接口化。
- [ ] 对照 `oldsrc/KernelCL.cl` 与 `kernels/*.cu`，理解 OpenCL 到 CUDA 的迁移方向（注意当前管线接入状态）。

<a id="sec-7-h"></a>

### H. 测试驱动阅读（每读完一块就验证）

- [ ] 运行 `ctest --test-dir build/clang-cl-debug -R test_view_model_replay --output-on-failure`，验证 UI 回放链路理解。
- [ ] 运行 `ctest --test-dir build/clang-cl-debug -R "ImageProcessor|OpenCvProcessingStrategyTest" --output-on-failure`，验证处理链理解。
- [ ] 运行 `ctest --test-dir build/clang-cl-debug -R "ManualTracker|DataExchange|TrackDataStorageBackend" --output-on-failure`，验证跟踪/通信/存储链路理解。
- [ ] 浏览 `tests/CMakeLists.txt`，建立“模块 -> 对应测试”索引，不再盲读源码。
- [ ] 精读 `tests/test_view_model_replay.cpp`、`tests/test_image_processor.cpp`、`tests/test_opencv_processing.cpp`、`tests/test_manual_tracker.cpp`、`tests/test_data_exchange.cpp`、`tests/test_track_data_storage_backend.cpp`。

<a id="sec-7-i"></a>

### I. 每完成一组模块，必须产出（防止“看过=会了”）

- [ ] 产出一张调用链图（至少 8 个节点，节点标注文件路径）。
- [ ] 产出一份术语表（至少包含 `IFrameSource`、`IProcessingStrategy`、`ITrackingStrategy`、`TrackResultEvent`、`ServiceRegistry`）。
- [ ] 产出一份“我现在能解释”的闭环说明（例如“回放一帧如何显示到 UI”）。
- [ ] 产出一份“未完成能力清单”（从 `docs/migration-status.md` 映射到具体文件）。
