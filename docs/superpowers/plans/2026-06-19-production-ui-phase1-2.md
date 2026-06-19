# 生产化与 UI 第二阶段重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成存储会话、滚动日志、诊断聚合、通信页组件化和可持久化设置页。

**Architecture:** 在现有 backend、Config、EventBus 和 ViewModel 边界上增量扩展。生产能力放在 core/storage/network，Qt 页面只依赖 ViewModel 与可复用面板。

**Tech Stack:** C++23、Qt6 Widgets、spdlog、nlohmann_json、GoogleTest、CMake、CTest。

---

### Task 1: 配置与滚动日志

**Files:** `include/dss/core/config.h`、`src/core/config.cpp`、`include/dss/core/logger.h`、`src/core/logger.cpp`、`config/settings.json`、`tests/test_config.cpp`、`tests/test_logger.cpp`

- [ ] 增加配置加载/保存失败测试，覆盖日志默认值与 JSON 往返。
- [ ] 增加 rotating sink 配置测试并确认旧 API 无法满足测试。
- [ ] 实现 `LoggingConfig` 与可重复配置的滚动文件 sink。
- [ ] 运行 `test_config` 和 `test_logger`。

### Task 2: 存储会话与错误事件

**Files:** `include/dss/core/events.h`、`include/dss/storage/local_image_storage_backend.h`、`include/dss/storage/track_data_storage_backend.h`、对应 storage 测试。

- [ ] 增加图像会话测试，验证 IMI、RAW/BMP 文件和统计。
- [ ] 增加轨迹会话测试，验证 GAE 路径和内容。
- [ ] 增加不可写路径测试，验证 `StorageWriteErrorEvent`。
- [ ] 实现显式会话 API、统一写入结果和错误事件。
- [ ] 运行 storage 相关测试。

### Task 3: 通信诊断聚合

**Files:** `include/dss/network/error_diagnostics.h`、`src/network/error_diagnostics.cpp`、`tests/test_error_diagnostics.cpp`、`tests/CMakeLists.txt`

- [ ] 增加事件到诊断状态映射测试并确认失败。
- [ ] 订阅网络、串口和存储错误事件，维护线程安全状态。
- [ ] 运行诊断测试和网络协议测试。

### Task 4: 通信页拆分

**Files:** `include/dss/ui/communication_panels.h`、`src/ui/serial_channels_panel.cpp`、`src/ui/serial_commands_panel.cpp`、`src/ui/network_endpoints_panel.cpp`、`src/ui/main_window_comm_page.cpp`、`tests/test_main_window_layout.cpp`

- [ ] 扩展 UI 构造测试，要求三个面板存在稳定 objectName。
- [ ] 将编辑器状态、工厂函数和刷新连接分别迁入三个 QWidget。
- [ ] 将 `setupCommStatusPage()` 收敛为页面组装函数。
- [ ] 运行 UI 布局和 ViewModel 测试。

### Task 5: 设置 ViewModel 与设置页

**Files:** `include/dss/ui/settings_view_model.h`、`src/ui/settings_view_model.cpp`、`include/dss/ui/main_view_model.h`、`src/ui/main_view_model.cpp`、`src/ui/main_window_log_settings_pages.cpp`、`tests/test_settings_view_model.cpp`

- [ ] 增加设置加载、输入校验和保存测试并确认失败。
- [ ] 实现 SettingsViewModel 并接入 MainViewModel。
- [ ] 用路径、日志、光学表单替换占位设置页。
- [ ] 运行设置与主窗口测试。

### Task 6: 应用接线与全量验证

**Files:** `src/app/application_context.cpp`、`src/app/communication_services.cpp`、`src/main.cpp`、相关集成测试与迁移文档。

- [ ] 配置日志、存储事件总线和会话默认值。
- [ ] 更新中文迁移状态文档。
- [ ] 运行 `cmake --build --preset msvc-debug`。
- [ ] 运行 `ctest --test-dir build\\msvc-debug --output-on-failure`。
- [ ] 执行 `git diff --check` 和最终代码审查。

