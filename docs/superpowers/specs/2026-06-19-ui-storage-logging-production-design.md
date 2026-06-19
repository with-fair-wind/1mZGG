# UI 拆分、存储背压与文件日志设计

## 目标

在不改变 UI 行为的前提下减小 `MainWindow` 单文件体积，并为高帧率运行补上第一批生产化的存储与日志保护能力。

## 范围

本切片包含三个边界明确的改动：

1. 将 `src/ui/main_window.cpp` 中的页面构建代码拆到更聚焦的实现文件中，同时保持 `MainWindow` 的公开接口和运行时行为不变。
2. 为本地图像存储和轨迹数据存储 worker 增加有界队列与背压状态报告。
3. 为现有 logger 增加可选文件日志，同时保留通过事件总线投递日志到 UI 日志页的能力。

本切片不新增 UI 页面，不调整视觉布局，不实现 BMP/IFM/IMI/GAE 完整会话格式，不添加日志搜索/导出，也不完成 CUDA/Sapera 集成。

## UI 架构

`MainWindow` 仍然是 Qt widget 的拥有者和信号连接点。页面构建代码按职责移动到独立 `.cpp` 文件：

- `main_window_control_page.cpp`：回放、处理、跟踪、曝光和保存控件。
- `main_window_display_page.cpp`：图像显示和显示拉伸控件。
- `main_window_comm_page.cpp`：串口通道、网络端点和命令面板。
- `main_window_log_settings_pages.cpp`：现有分析页、现有设置页和日志页。

声明仍保留在 `include/dss/ui/main_window.h`。现有 private helper 方法保持原名，测试和调用点不需要变化。这次拆分是机械拆分：不改变行为，不新增 widget，不改变信号语义。

## 存储架构

`LocalImageStorageBackend` 和 `TrackDataStorageBackend` 已经使用后台 worker 与队列。本切片通过以下能力让队列更适合生产环境：

- 增加可配置的最大待处理队列长度，并提供保守默认值。
- 增加丢弃请求计数器，并通过 const accessor 暴露。
- 当 worker 未启动或队列已满时，enqueue 返回失败。

存储 worker 在停止时仍然 drain 已接受的请求。队列满时拒绝新的请求，避免高帧率场景下内存无界增长。

## 日志架构

现有 `Logger` 继续发布 `LogMessageEvent`，供 UI 日志页消费。本切片增加可选文件输出：

- 文件日志需要通过路径显式启用。
- 尽可能自动创建父目录。
- 文件 sink 配置失败时通过 `std::expected` 返回错误。
- 无论文件日志是否启用，事件总线日志都保持可用。

实现上应避免让测试依赖进程全局日志状态。测试需要使用临时目录隔离输出，并在需要时显式配置、写入、flush 或 shutdown。

## 错误处理

UI 拆分只应产生编译期风险；运行时行为必须保持不变。

存储 enqueue 方法在无法接受任务时返回 `false`。丢弃计数器让 ViewModel 或后续诊断功能可以暴露背压状态，本切片不改变当前 UI。

Logger 文件配置返回 `std::expected<void, std::string>`，调用者可以自行决定文件日志失败是否致命。文件配置失败不应关闭已有事件总线日志。

## 测试

行为改动使用 TDD：

- 增加存储测试：填满每个 backend 队列，验证下一次 enqueue 被拒绝且丢弃计数增加。
- 增加 logger 测试：启用文件日志，写入消息，并验证文件包含预期记录。
- 保留现有 UI 测试，作为 `MainWindow` 机械拆分的回归保护。

验证目标：`ctest --test-dir build\\msvc-debug --output-on-failure` 继续保持绿色。

## 风险

UI 拆分会触碰大文件，主要风险是误改信号连接或 widget ownership。保持函数名和声明稳定可以降低这个风险。

存储背压变化可能影响假设 enqueue 总是成功的调用方。现有测试已经覆盖 worker 停止时的拒绝行为，新测试会记录队列满时的拒绝语义。

Logger 改动涉及进程全局日志。测试必须隔离文件路径，并避免依赖特定的全局 sink 顺序。
