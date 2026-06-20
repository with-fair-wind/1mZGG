# 后续框架重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在已完成 UI 拆分、存储与日志生产化的基础上，依次打通业务会话、处理管线、完整跟踪、真实硬件和 CUDA 加速，并最终退出 `oldsrc/` 迁移期。

**Architecture:** 继续保持 `ApplicationContext` 为组合根、`MessageBus` 为后端事件通道、子 ViewModel 为 UI 边界。后续能力按“纯数据模型与策略 -> 服务接线 -> UI 命令 -> 集成测试”顺序推进，每一阶段都能在无硬件环境独立验收。

**Tech Stack:** C++23、Qt 6 Widgets、GoogleTest/CTest、OpenCV、spdlog、可选 Sapera SDK、可选 CUDA。

---

## 总体顺序

| 阶段 | 目标 | 优先级 | 完成标志 |
|---|---|---:|---|
| 第三阶段 | 业务会话与处理闭环 | P0 | 主控任务字段驱动 BMP/IMI/GAE 会话；Diff/OpenCV 参数可配置；回放可定位 |
| 第四阶段 | 跟踪算法完整化 | P0 | GEO/LEO/SC/Manual 关键 legacy 分支有黄金数据回归，TWDW/GDCL 字段来源明确 |
| 第五阶段 | 真实硬件适配 | P1 | Sapera 和相机串口作为可选适配器接入，默认仍保持无硬件安全 |
| 第六阶段 | CUDA、性能与迁移收口 | P1 | CUDA 策略可选启用，有 CPU 对照基准；`oldsrc/` 只保留归档或移除 |

---

## 第三阶段：业务会话与处理闭环

### Task 0: 校准迁移文档基线

**Files:**
- Modify: `docs/migration-status.md`
- Modify: `docs/module-storage.md`
- Modify: `docs/module-ui.md`
- Modify: `docs/module-app.md`

- [x] **Step 1: 删除已经完成但仍标记待补的描述**

统一修正日志持久化/搜索/导出、设置页、诊断聚合、BMP/IMI/GAE 会话存储和通信页拆分状态。

- [x] **Step 2: 把本文档的第三至第六阶段链接加入迁移状态页**

- [x] **Step 3: 检查中文编码和 Markdown 格式后提交**

Run: `git diff --check`

```bash
git add docs
git commit -m "docs: align migration status with completed phases"
```

### Task 1: 观测会话模型与存储编排

**Files:**
- Create: `include/dss/app/observation_session.h`
- Create: `src/app/observation_session.cpp`
- Modify: `include/dss/core/config.h`
- Modify: `src/core/config.cpp`
- Modify: `config/SystemInit.json`
- Modify: `include/dss/ui/storage_view_model.h`
- Modify: `src/ui/storage_view_model.cpp`
- Modify: `include/dss/ui/main_view_model.h`
- Modify: `src/ui/main_view_model.cpp`
- Test: `tests/test_observation_session.cpp`
- Test: `tests/test_storage_view_model.cpp`
- Test: `tests/test_main_view_model.cpp`
- Test: `tests/test_config.cpp`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: 为主控任务到会话命名的映射编写失败测试**

测试应验证 `taskId`、`targetId`、`start/end`、台站编号和搜索模式被转换为 `ImageStorageNaming`，并验证非法时间或空台站编号返回 `std::unexpected`。

```cpp
const Dss::Core::MasterControlEvent command{
    .mode1 = 1,
    .targetId = 42,
    .taskId = 7,
    .start = {.hour = 8, .minute = 30, .second = 0},
    .end = {.hour = 9, .minute = 30, .second = 0},
};
const auto observationDate =
    std::chrono::sys_days{std::chrono::year{2026} / std::chrono::June / 20};
const auto session =
    Dss::App::makeObservationSession(command, "999", observationDate);
ASSERT_TRUE(session);
EXPECT_EQ(session->naming.taskId, "7");
EXPECT_EQ(session->naming.targetId, "42");
```

- [x] **Step 2: 运行测试并确认因接口不存在而失败**

Run: `cmake --build --preset msvc-debug --target test_observation_session && ctest --test-dir build/msvc-debug -R "ObservationSession" --output-on-failure`

- [x] **Step 3: 实现不可变 `ObservationSession` 与映射函数**

`ObservationSession` 只保存规范化后的会话 ID、`ImageStorageNaming` 和来源，不持有存储后端或 Qt 对象。台站编号进入 `ObservatoryConfig`，不要继续在 `StorageViewModel` 中硬编码 `"0"`。

- [x] **Step 4: 让 `StorageViewModel::startSaving()` 接收会话配置**

增加 `startSaving(const ImageStorageNaming&)`，保留无参数重载作为本地回放默认会话；先配置图像和轨迹后端，再启动两个 worker，任一失败时回滚已启动资源。

- [x] **Step 5: 在 `MainViewModel::onMasterControl()` 中完成业务接线**

当 `event.save` 从 false 变为 true 时创建会话并启动保存；任务字段变化时不得静默覆盖运行中会话，应先停止旧会话再显式创建新会话。

- [x] **Step 6: 运行会话、存储和主 ViewModel 测试**

Run: `ctest --test-dir build/msvc-debug -R "ObservationSession|StorageViewModel|MainViewModel" --output-on-failure`

- [x] **Step 7: 提交**

```bash
git add include/dss/app src/app include/dss/ui src/ui tests
git commit -m "refactor: add observation session orchestration"
```

### Task 2: Diff 处理策略与参数化处理配置

**Files:**
- Create: `include/dss/processing/diff_processing_strategy.h`
- Create: `src/processing/diff_processing_strategy.cpp`
- Modify: `include/dss/core/config.h`
- Modify: `src/core/config.cpp`
- Modify: `include/dss/processing/opencv_processing_strategy.h`
- Modify: `src/processing/opencv_processing_strategy.cpp`
- Modify: `include/dss/ui/processing_view_model.h`
- Modify: `src/ui/processing_view_model.cpp`
- Modify: `src/ui/main_window_control_page.cpp`
- Test: `tests/test_diff_processing.cpp`
- Test: `tests/test_processing_view_model.cpp`
- Test: `tests/test_config.cpp`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: 用两帧固定矩阵定义 Diff 策略行为**

测试覆盖首帧无差分、第二帧绝对差、阈值分割、尺寸变化重置历史帧和无效像素数量拒绝。

```cpp
Dss::Processing::DiffProcessingStrategy strategy({.threshold = 20});
const auto first = strategy.process(makeFrame({10, 10, 10, 10}, 2, 2));
const auto second = strategy.process(makeFrame({10, 40, 10, 40}, 2, 2));
EXPECT_TRUE(first.targetBlobs.empty());
EXPECT_EQ(second.displayImage, std::vector<std::uint8_t>({0, 255, 0, 255}));
```

- [x] **Step 2: 实现 CPU Diff 策略并复用 `labelAndExtract()`**

策略内部只保留上一帧 16 位数据；阈值和最小目标面积由构造参数注入，不读取全局单例。

- [x] **Step 3: 将 OpenCV/Diff 参数加入 `Config` JSON 往返测试**

配置至少包含阈值、最小目标面积、最大目标面积和显示拉伸参数；无效值回落到安全默认值。

- [x] **Step 4: 扩展 `ProcessingViewModel` 的策略工厂**

ViewModel 根据 `ProcessingMode::Direct`/`Diff` 创建策略，并把参数作为快照传入；切换策略时不让 UI 直接持有 backend。

- [x] **Step 5: 运行处理管线测试**

Run: `ctest --test-dir build/msvc-debug -R "DiffProcessing|OpenCvProcessing|ProcessingPipeline|ProcessingViewModel|ConfigTest" --output-on-failure`

- [x] **Step 6: 提交**

```bash
git add include/dss/core include/dss/processing include/dss/ui src tests config/SystemInit.json
git commit -m "feat: add configurable diff processing strategy"
```

### Task 3: 回放随机访问与端到端运行统计

**Files:**
- Modify: `include/dss/acquisition/image_sequence_frame_source.h`
- Modify: `src/acquisition/image_sequence_frame_source.cpp`
- Modify: `include/dss/ui/replay_view_model.h`
- Modify: `src/ui/replay_view_model.cpp`
- Modify: `src/ui/main_window_control_page.cpp`
- Create: `include/dss/app/runtime_diagnostics.h`
- Create: `src/app/runtime_diagnostics.cpp`
- Test: `tests/test_image_sequence_frame_source.cpp`
- Test: `tests/test_replay_view_model.cpp`
- Test: `tests/test_runtime_diagnostics.cpp`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: 为 `seek(index)`、后退和运行中定位编写失败测试**

定义索引越界返回错误，暂停时定位不自动播放，运行中定位从目标帧继续且不重复旧帧。

- [x] **Step 2: 在线程互斥边界内实现帧索引状态机**

把当前索引、运行/暂停和单步请求集中在帧源内部；ViewModel 只暴露命令和进度属性。

- [x] **Step 3: 聚合处理、存储和通信统计**

`RuntimeDiagnostics` 订阅错误事件并读取 `ImageProcessor::droppedFrames()`、存储成功/失败/背压计数，输出线程安全快照；不要让诊断服务依赖 QWidget。

- [x] **Step 4: 在控制页增加稳定尺寸的进度条和诊断区**

UI 只绑定 `ReplayViewModel` 和诊断快照，避免再把业务逻辑写回 `MainWindow`。

- [x] **Step 5: 运行回放与诊断测试并提交**

Run: `ctest --test-dir build/msvc-debug -R "ImageSequenceFrameSource|ReplayViewModel|RuntimeDiagnostics" --output-on-failure`

```bash
git add include/dss/acquisition include/dss/app include/dss/ui src tests
git commit -m "feat: add replay seeking and runtime diagnostics"
```

---

## 第四阶段：跟踪算法完整化

### Task 4: 建立 legacy 黄金数据回归夹具

**Files:**
- Create: `tests/fixtures/tracking/README.md`
- Create: `tests/include/tracking_fixture_loader.h`
- Create: `tests/tracking_fixture_loader.cpp`
- Create: `tests/test_tracking_legacy_scenarios.cpp`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: 从 `oldsrc/TrackAlgo.cpp` 提取最小输入输出场景**

每个 fixture 使用可审查 JSON，包含帧号、blob、AE/RA-Dec、期望目标 ID、living、TWDW 和 GDCL 字段；不序列化旧类内存布局。

- [x] **Step 2: 实现 fixture loader 并验证坏格式、缺字段和数值边界**

- [x] **Step 3: 为 GEO/LEO/SC/Manual 各建立至少一个连续命中、连续 miss、重发现场景**

- [x] **Step 4: 运行 `test_tracking_legacy_scenarios` 并提交夹具基线**

### Task 5: 补齐 GEO 外部校验与 TWDW/GDCL 数据源

**Files:**
- Modify: `include/dss/tracking/prediction_utils.h`
- Modify: `src/tracking/prediction_utils.cpp`
- Modify: `include/dss/tracking/geo_tracker.h`
- Modify: `src/tracking/geo_tracker.cpp`
- Create: `src/tracking/geo_association.cpp`
- Create: `src/tracking/geo_validation.cpp`
- Create: `src/tracking/geo_continuous_tracking.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/core/result_packet_utils.cpp`
- Test: `tests/test_tracking_prediction_utils.cpp`
- Test: `tests/test_geo_tracker.cpp`
- Test: `tests/test_result_packet_utils.cpp`

- [x] **Step 1: 用黄金场景锁定外部校验 blob 的生成条件和 ID 规则**
- [x] **Step 2: 将校验 blob 生成提取为 Qt-free 纯函数**
- [x] **Step 3: 补齐 TWDW/GDCL 的位置、速度、DN、星等和距离来源**
- [x] **Step 4: 将 `geo_tracker.cpp` 的候选生成、验证和持续跟踪拆成三个私有策略单元，使主循环低于 250 行**
- [x] **Step 5: 运行 GEO、预测、结果包和数据交换测试并提交**

### Task 6: 补齐 LEO、SC 与 Manual legacy 分支

**Files:**
- Modify: `src/tracking/leo_tracker.cpp`
- Modify: `src/tracking/sc_tracker.cpp`
- Modify: `src/tracking/manual_tracker.cpp`
- Modify: `src/tracking/candidate_utils.cpp`
- Modify: `src/tracking/lifecycle_utils.cpp`
- Test: `tests/test_leo_tracker.cpp`
- Test: `tests/test_sc_tracker.cpp`
- Test: `tests/test_manual_tracker.cpp`
- Test: `tests/test_tracking_legacy_scenarios.cpp`

- [x] **Step 1: 按黄金数据逐模式补 TWDW/GDCL 字段，不复制 GEO 分支逻辑**
- [x] **Step 2: 将公共匹配、预测和生命周期行为继续下沉到现有 helper**
- [x] **Step 3: 增加跨模式 ID 唯一性、目标释放和重发现回归**
- [x] **Step 4: 运行全部 tracking 与 network protocol 测试并提交**

---

## 第五阶段：真实硬件适配

### Task 7: 帧源协调器与相机串口适配器

**Files:**
- Create: `include/dss/acquisition/frame_source_coordinator.h`
- Create: `src/acquisition/frame_source_coordinator.cpp`
- Create: `include/dss/acquisition/serial_camera_controller.h`
- Create: `src/acquisition/serial_camera_controller.cpp`
- Modify: `src/app/communication_services.cpp`
- Modify: `include/dss/ui/replay_view_model.h`
- Test: `tests/test_frame_source_coordinator.cpp`
- Test: `tests/test_serial_camera_controller.cpp`

- [ ] **Step 1: 测试同一时间只能有一个活动帧源**
- [ ] **Step 2: 实现 replay/live 的显式切换和失败回滚**
- [ ] **Step 3: 让相机控制器通过窄串口发送端口发送已有协议字节**
- [ ] **Step 4: 保持默认启动不打开串口、不启动采集**
- [ ] **Step 5: 运行无硬件集成测试并提交**

### Task 8: 可选 Sapera `IFrameSource`

**Files:**
- Create: `include/dss/acquisition/sapera_frame_source.h`
- Create: `src/acquisition/sapera_frame_source.cpp`
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `src/app/communication_services.cpp`
- Test: `tests/test_sapera_frame_source_contract.cpp`

- [ ] **Step 1: 定义 `DSS_ENABLE_SAPERA=OFF` 时的构建与注册契约**
- [ ] **Step 2: 用 RAII 包装 Sapera 资源，回调只负责复制/转交 `FramePacket`**
- [ ] **Step 3: 将 SDK 错误转换为 `std::expected` 和统一采集错误事件**
- [ ] **Step 4: 在无 SDK preset 下跑完整测试，在硬件环境跑显式 smoke test**

---

## 第六阶段：CUDA、性能与迁移收口

### Task 9: CUDA 处理策略与 CPU 对照

**Files:**
- Create: `include/dss/processing/cuda_processing_strategy.h`
- Create: `src/processing/cuda_processing_strategy.cpp`
- Modify: `src/gpu/cuda_device_manager.cpp`
- Modify: `src/ui/processing_view_model.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_cuda_processing_contract.cpp`
- Create: `benchmarks/processing_benchmark.cpp`

- [ ] **Step 1: 定义 CUDA 不可用时策略创建失败而应用继续运行的契约**
- [ ] **Step 2: 用 `GpuBuffer` 和 `CudaDeviceManager` 封装 kernel 生命周期**
- [ ] **Step 3: 对固定帧验证 CPU/OpenCV/CUDA blob 输出容差**
- [ ] **Step 4: 对 6144 级图像记录吞吐、延迟、显存和丢帧指标**
- [ ] **Step 5: 仅在基准证明收益后把 CUDA 暴露为 UI 选项**

### Task 10: 大文件继续拆分与 `oldsrc` 退出

**Files:**
- Split: `src/tracking/geo_tracker.cpp`
- Split: `include/dss/core/event_bus.h`
- Split: `include/dss/comm/serial_protocol_codec.h`
- Modify: `docs/migration-status.md`
- Modify: `docs/module-*.md`
- Modify: `src/main.cpp`

- [ ] **Step 1: 只拆分仍在频繁修改且超过 500 行的文件**

`geo_tracker.cpp` 按关联/验证/持续跟踪拆分；`event_bus.h` 按连接、锁策略和总线模板拆分；串口 codec 按通道协议拆分。每次拆分必须是纯结构提交，不混入行为修改。

- [ ] **Step 2: 删除本轮新增完成事项的过时 TODO 和文档描述**

重点修正日志持久化/搜索、设置页、诊断聚合和存储会话已经完成但文档仍标记待补的条目。

- [ ] **Step 3: 建立 `oldsrc` 能力到新模块/测试的最终映射表**

- [ ] **Step 4: 全量验证后将 `oldsrc/` 转为只读归档或从主构建仓库移除**

Run: `cmake --build --preset msvc-debug && ctest --test-dir build/msvc-debug --output-on-failure`

Run: `git diff --check`

---

## 阶段门禁

每个阶段必须同时满足：

1. 新行为先有失败测试，再有实现。
2. 无硬件、无 Sapera、无 CUDA 的默认 preset 始终可构建运行。
3. 不把业务逻辑回填到 `MainWindow`；UI 页面只做控件创建和信号绑定。
4. 不直接翻译旧版 3000 行算法；先提取场景、纯函数和可比较输出。
5. 每个任务独立提交，阶段结束运行全量 CTest 和 `git diff --check`。

## 暂不进入本轮的事项

- 自研主题系统和大规模视觉改版。
- 为未来可能需求改写 EventBus 为另一套框架。
- 在缺少黄金数据前重写完整跟踪数学模型。
- 在 CPU 正确性与基准未稳定前进行 CUDA 微优化。
