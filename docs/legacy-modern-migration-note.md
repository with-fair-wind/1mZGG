# DSS_QT 旧新架构阅读指南

本文档用于阅读 `oldsrc/` 与现代实现之间的结构变化。它不是迁移进度表；当前完成度以 [迁移状态](migration-status.md) 为准，旧新能力索引以 [Legacy 能力最终映射](legacy-capability-map.md) 为准。

## 1. 从单体入口到组合根

旧版 `oldsrc/main.cpp` 直接实例化通信、采集、处理和 UI 对象，并集中书写信号连接。现代入口 `src/main.cpp` 只管理 Qt 生命周期、配置加载、`ApplicationContext` 服务注册和窗口展示。

重点阅读：

- `include/dss/app/application_context.h`
- `src/app/application_context.cpp`
- `src/app/communication_services.cpp`
- `include/dss/core/service_registry.h`
- `src/main.cpp`

`ApplicationContext::registerCommunicationServices()` 集中注册串口、网络、处理、回放、存储、诊断和可选 Sapera 服务。默认注册不会打开硬件；相机服务当前使用 `CommandOnlyCameraController`。

## 2. 从全局事件连接到分层事件

- 后端模块通过 `BasicMessageBus` 传递帧、处理结果、跟踪结果和错误事件。
- UI 通过 `MainViewModel` 与各子 ViewModel 订阅后端事件并暴露 Qt 状态。
- `AppEvent` 只承担 UI 层跨控件事件，不替代后端消息总线。

重点阅读：

- `include/dss/core/event_bus.h`
- `include/dss/core/events.h`
- `include/dss/ui/main_view_model.h`
- `include/dss/ui/view_model_context.h`
- `src/ui/main_view_model.cpp`
- `include/dss/ui/app_event.h`

## 3. 从大 ViewModel 到子模块

旧 `ViewModel` facade 已移除。现代 UI 由 `MainViewModel` 组合以下子模块：

- `ReplayViewModel`
- `DisplayViewModel`
- `ProcessingViewModel`
- `TrackingViewModel`
- `StorageViewModel`
- `SerialPortViewModel`
- `NetworkViewModel`
- `DataExchangeViewModel`
- `SettingsViewModel`
- `LogViewModel`

页面创建与信号绑定拆分在 `main_window_control_page.cpp`、`main_window_comm_page.cpp`、`main_window_log_settings_pages.cpp` 等文件中。阅读 UI 命令时从页面文件找到子 ViewModel 方法，再回到对应 `*_view_model.cpp`。

## 4. 回放、处理与显示闭环

```text
ImageSequenceFrameSource
        │ FramePacket
        ▼
FrameSourceCoordinator
        ▼
ImageProcessor ──► ProcessingPipeline
        │
        ├── DisplayRefreshEvent ──► DisplayViewModel / ImageDisplay
        └── TrackResultEvent ─────► TrackingViewModel / Storage / DataExchange
```

重点阅读：

- `include/dss/acquisition/i_frame_source.h`
- `src/acquisition/image_sequence_frame_source.cpp`
- `src/acquisition/frame_source_coordinator.cpp`
- `src/processing/image_processor.cpp`
- `src/processing/processing_pipeline.cpp`
- `src/processing/diff_processing_strategy.cpp`
- `src/processing/opencv_processing_strategy.cpp`

回放支持暂停、前进/后退、随机定位与运行中 seek。处理策略支持 Direct、OpenCV 和 Diff；CUDA 是显式启用并需要硬件验收的可选策略。

## 5. 从 TrackAlgo 大单体到策略

旧版 `TrackAlgo.cpp` 同时包含 GEO、LEO、SC、Manual 以及关联、验证和持续跟踪。现代实现拆为：

- `geo_tracker.cpp` 及 `geo_association.cpp`、`geo_validation.cpp`、`geo_continuous_tracking.cpp`
- `leo_tracker.cpp`
- `sc_tracker.cpp`
- `manual_tracker.cpp`
- `candidate_utils`、`prediction_utils`、`lifecycle_utils` 公共规则

GEO/LEO/SC 的关联、验证、持续跟踪、miss、释放和重发现关键路径已有回归覆盖。Manual 已形成选点到结果输出闭环，完整 legacy 历史校验语义仍是明确剩余项。

## 6. 通信、存储与结果桥接

重点阅读顺序：

1. `serial_protocol_codec.h` 与四路 Channel。
2. `data_exchange_protocol.h` 与 `DataExchange`。
3. `track_result_data_exchange_bridge.cpp`，理解 `TrackResultEvent` 到 GXTC/GDCL 的映射。
4. `local_image_storage_backend.h` 与 `track_data_storage_backend.h`，理解异步队列、背压和会话写入。
5. `observation_session.cpp`，理解主控任务到存储命名的映射。

相机命令需要区分两层：`SerialCameraController` 发送适配器已实现并测试，但默认组合根仍使用不打开串口的 `CommandOnlyCameraController`。

## 7. 测试驱动阅读

```powershell
ctest --test-dir build/clang-cl-debug -R "test_replay_view_model|test_image_sequence_frame_source" --output-on-failure
ctest --test-dir build/clang-cl-debug -R "ImageProcessor|DiffProcessing|OpenCvProcessing" --output-on-failure
ctest --test-dir build/clang-cl-debug -R "GeoTracker|LeoTracker|ScTracker|ManualTracker|TrackingLegacy" --output-on-failure
ctest --test-dir build/clang-cl-debug -R "DataExchange|ObservationSession|StorageBackend" --output-on-failure
```

对应测试源文件：

- `tests/test_replay_view_model.cpp`
- `tests/test_image_sequence_frame_source.cpp`
- `tests/test_image_processor.cpp`
- `tests/test_diff_processing.cpp`
- `tests/test_opencv_processing.cpp`
- `tests/test_geo_tracker.cpp`、`test_leo_tracker.cpp`、`test_sc_tracker.cpp`、`test_manual_tracker.cpp`
- `tests/test_tracking_legacy_scenarios.cpp`
- `tests/test_data_exchange.cpp`
- `tests/test_observation_session.cpp`
- `tests/test_local_image_storage_backend.cpp`
- `tests/test_track_data_storage_backend.cpp`

## 8. 阅读检查清单

- [ ] 能从 `main.cpp` 追到所有服务注册 key。
- [ ] 能解释默认启动为什么不会打开串口、UDP、Sapera 或 CUDA。
- [ ] 能从控制页信号追到对应子 ViewModel 和后端服务。
- [ ] 能画出回放帧到显示、跟踪、存储和网络结果的路径。
- [ ] 能说明 GEO/LEO/SC/Manual 的共同接口与差异。
- [ ] 能区分已实现组件、默认运行时接线和硬件现场验收三种状态。
- [ ] 能为修改点找到对应的 CTest 目标和测试源文件。
