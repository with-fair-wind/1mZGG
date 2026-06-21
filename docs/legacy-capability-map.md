# Legacy 能力最终映射

本文档是 `oldsrc/` 只读归档与现代模块之间的最终索引。新功能、缺陷修复和测试不得继续写入 `oldsrc/`。

| Legacy 能力 | 当前实现 | 主要验证 |
|---|---|---|
| `Grabber` Sapera 采集 | `SaperaFrameSource`、`ISaperaCaptureSession`、`FrameSourceCoordinator` | `test_sapera_frame_source_contract`、`test_frame_source_coordinator` |
| `CommCamera` 相机命令/串口 | `camera_control_protocol.h`、`SerialCameraController`；默认组合根使用 `CommandOnlyCameraController` | 编码与发送适配器测试已完成，具体运行时串口接线待补 |
| `ImageReplayer` | `ImageSequenceFrameSource`、`ReplayViewModel` | `test_image_sequence_frame_source`、`test_replay_view_model` |
| Direct/Diff 处理 | `OpenCvProcessingStrategy`、`DiffProcessingStrategy` | `test_opencv_processing`、`test_diff_processing` |
| CUDA 图像处理 | `CudaProcessingStrategy`、`CudaDeviceManager`、`GpuBuffer` | `test_cuda_processing_contract`、`dss_processing_benchmark` |
| `TrackAlgo` | 各 Tracker、GEO 拆分单元与公共 helper | 模式单测、跨模式协议回归、legacy JSON fixtures |
| 串口协议 | `serial_protocol_codec.h` 与 `comm/detail` helper | `test_serial_protocol_codec`、通道测试 |
| 事件总线 | `event_bus.h` 与 `core/detail` 模板层 | logger、diagnostics、application context 测试 |
| 图像/轨迹存储 | 两个异步 StorageBackend | 格式、会话、背压及错误事件测试 |

## 归档规则

- `oldsrc/` 不参与任何 CMake 目标，也不作为运行时资源。
- 仅允许补充归档说明，不再修改其中算法或 UI 源码。
- 行为差异以现代模块测试和 `tests/fixtures/tracking/` 黄金数据为准。
- Sapera 与 CUDA 的硬件结果必须由显式 preset/基准产生。