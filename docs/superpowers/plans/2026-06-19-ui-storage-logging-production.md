# UI 拆分、存储背压与文件日志实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保持现有 UI 行为不变的前提下拆分 `MainWindow` 大文件，并为存储 worker 与 logger 增加第一批生产化保护能力。

**Architecture:** UI 只做机械拆分，保留 `MainWindow` 方法名和头文件声明。存储后端在现有队列入口加容量上限和丢弃计数。Logger 在现有 spdlog sink 组合上追加可选 basic file sink。

**Tech Stack:** C++23、Qt6 Widgets、spdlog、GoogleTest、CTest、CMake。

---

### Task 1: 存储队列背压

**Files:**
- Modify: `include/dss/storage/local_image_storage_backend.h`
- Modify: `include/dss/storage/track_data_storage_backend.h`
- Test: `tests/test_local_image_storage_backend.cpp`
- Test: `tests/test_track_data_storage_backend.cpp`

- [ ] **Step 1: 写 LocalImageStorageBackend 队列满的失败测试**

在 `tests/test_local_image_storage_backend.cpp` 增加测试：构造容量为 1 的 backend，启动 worker 前填充第一帧后立即提交第二帧，期望第二帧被拒绝且 `droppedRequests() == 1`。

- [ ] **Step 2: 运行单测确认失败**

Run: `cmake --build --preset msvc-debug --target test_local_image_storage_backend`

Expected: 编译失败或测试失败，提示构造函数/`droppedRequests` 不存在。

- [ ] **Step 3: 实现 LocalImageStorageBackend 背压**

为构造函数增加可选 `maxPendingRequests` 参数，新增 `droppedRequests()`，在 `enqueueRawFrame` 持锁后检查 `m_queue.size() >= m_maxPendingRequests`，满时递增计数并返回 `std::unexpected("storage queue is full")`。

- [ ] **Step 4: 写 TrackDataStorageBackend 队列满的失败测试**

在 `tests/test_track_data_storage_backend.cpp` 增加同类测试：容量为 1，第二次 `enqueueTrackResult` 被拒绝且 `droppedRequests() == 1`。

- [ ] **Step 5: 实现 TrackDataStorageBackend 背压**

与图像存储一致：构造函数可配置容量，新增 `droppedRequests()`，满队列拒绝新任务。

- [ ] **Step 6: 验证存储测试**

Run: `cmake --build --preset msvc-debug --target test_local_image_storage_backend test_track_data_storage_backend`

Run: `ctest --test-dir build\\msvc-debug --output-on-failure -R "LocalImageStorageBackend|TrackDataStorageBackend"`

Expected: 相关测试全部通过。

### Task 2: Logger 文件落盘

**Files:**
- Modify: `include/dss/core/logger.h`
- Modify: `src/core/logger.cpp`
- Test: `tests/test_logger.cpp`

- [ ] **Step 1: 写文件日志失败测试**

在 `tests/test_logger.cpp` 增加测试：启用临时目录下 `dss.log`，写入 info/warn/error，flush 后验证文件存在且包含三条消息。

- [ ] **Step 2: 运行 logger 单测确认失败**

Run: `cmake --build --preset msvc-debug --target test_logger`

Expected: 编译失败，提示 `enableFileLogging` 不存在。

- [ ] **Step 3: 实现文件日志**

在 `Logger` 增加 `enableFileLogging(const std::filesystem::path&) -> std::expected<void, std::string>`。实现中创建父目录，追加 `spdlog::sinks::basic_file_sink_mt` 到现有 logger sink 列表，保持 EventBus sink 和 console sink 不变。

- [ ] **Step 4: 验证 logger 测试**

Run: `cmake --build --preset msvc-debug --target test_logger`

Run: `ctest --test-dir build\\msvc-debug --output-on-failure -R LoggerTest`

Expected: logger 测试全部通过。

### Task 3: MainWindow 机械拆分

**Files:**
- Modify: `src/ui/main_window.cpp`
- Create: `src/ui/main_window_control_page.cpp`
- Create: `src/ui/main_window_display_page.cpp`
- Create: `src/ui/main_window_comm_page.cpp`
- Create: `src/ui/main_window_log_settings_pages.cpp`

- [ ] **Step 1: 移动控制页实现**

将 `MainWindow::setupControlPage` 从 `main_window.cpp` 移动到 `main_window_control_page.cpp`，补齐该文件需要的 Qt include 和 `app_event`/ViewModel include。

- [ ] **Step 2: 移动显示页实现**

将 `MainWindow::setupDisplayPage` 移动到 `main_window_display_page.cpp`。

- [ ] **Step 3: 移动通信页实现**

将 `MainWindow::setupCommStatusPage` 移动到 `main_window_comm_page.cpp`。

- [ ] **Step 4: 移动分析/设置/日志页实现**

将 `setupAnalysisPage`、`setupSettingsPage`、`setupLogPage` 和日志渲染 helper 移动到 `main_window_log_settings_pages.cpp`。

- [ ] **Step 5: 保留主文件职责**

`main_window.cpp` 只保留构造/析构、`makeScrollablePage`、`setupNavigation` 和 `connectSignals`。

- [ ] **Step 6: 验证 UI 构建和测试**

Run: `cmake --build --preset msvc-debug --target test_main_window_layout test_image_display test_view_model_tracking test_view_model_processing test_view_model_replay`

Run: `ctest --test-dir build\\msvc-debug --output-on-failure -R "test_main_window_layout|test_image_display|test_view_model"`

Expected: UI 相关测试全部通过。

### Task 4: 全量验证

**Files:**
- No code changes.

- [ ] **Step 1: 全量构建**

Run: `cmake --build --preset msvc-debug`

Expected: 构建成功。

- [ ] **Step 2: 全量测试**

Run: `ctest --test-dir build\\msvc-debug --output-on-failure`

Expected: 全部测试通过。
