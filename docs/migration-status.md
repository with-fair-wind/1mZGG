# oldsrc → 新架构迁移状态

> 最近更新：2026-06-21。
>
> `oldsrc/` 已转为只读归档，不参与 CMake 构建，也不再作为新功能或缺陷修复的落点。行为差异以现代模块测试和 `tests/fixtures/tracking/` 黄金数据为准。

## 当前结论

核心架构迁移已经完成：CMake 模块化构建、Core 类型与配置、串口和 UDP 通信、回放/实时帧源协调、异步存储、四种跟踪策略、Qt MVVM UI、可选 Sapera 适配器及可选 CUDA 处理策略均已有现代实现。

迁移状态不再按旧文件数量计算百分比。旧文件与现代模块并非一一对应，继续使用“文件数完成率”会把已拆分模块、可选硬件验证和明确放弃的 legacy 能力混在一起。以下以可验收能力为准。

| 领域 | 当前状态 | 仍需完成 |
|---|---|---|
| Core / 构建 | 已完成 | 无迁移阻塞项 |
| 串口通信 | 已完成主链路 | 断连重连、接收侧统计和更多字段诊断 |
| 网络通信 | 已完成主链路 | 发送样例、接收状态；ENet 尚未迁移 |
| 图像处理 | 部分完成 | 光度、星图、完整定标和指向误差能力 |
| 跟踪算法 | 关键分支已迁移 | Manual 完整 legacy 历史校验；其余模式继续按黄金数据校准边界行为 |
| 存储 | 已完成 | 无迁移阻塞项 |
| 采集 / 相机 | 回放、Sapera 与相机命令适配器已实现 | Sapera smoke test；相机串口默认组合根接线 |
| GPU | 策略与基准入口已完成 | CUDA 硬件正确性、性能收益和 UI 启用门槛验证 |
| UI | 主工作流已完成 | 距离曲线、CUDA 条件启用和部分联调状态展示 |

## 已完成能力

- CMake/CMakePresets、模块化库、C++23、Qt 6、GoogleTest/CTest。
- `Config` JSON 配置、`MessageBus`、`ServiceRegistry`、spdlog 文件日志和运行诊断。
- 四路串口协议与显式开关；图像发送、心跳、诊断、大气、GXTC/GDCL 网络服务。
- `ImageSequenceFrameSource` 回放，支持暂停续播、前进/后退、随机定位、运行中 seek 和进度快照。
- `FrameSourceCoordinator` 在 replay/live 间显式切换；默认启动不打开串口、UDP 或采集硬件。
- `LocalImageStorageBackend` 与 `TrackDataStorageBackend` 的会话写入、背压、drain 和错误事件；主控任务可映射到 `ObservationSession` 和会话命名。
- OpenCV 与 CPU Diff 处理策略；处理参数可配置并由 `ProcessingViewModel` 创建策略快照。
- GEO、LEO、SC 的关联、验证、持续跟踪、miss、释放和重发现关键路径；四种模式均有 legacy JSON 黄金场景。
- GXTC/GDCL 结果字段来源、协议映射、跟踪结果发送桥接和失败事件。
- `SaperaFrameSource`、`ISaperaCaptureSession`、帧源注册、无 SDK 安全构建和硬件 smoke 工具。
- `CudaProcessingStrategy`、`CudaDeviceManager`、GPU buffer 生命周期、固定帧契约测试和 6144 级基准入口。
- `MainViewModel` 与日志、回放、显示、处理、跟踪、存储、串口、网络、数据交换和设置子 ViewModel；旧 `ViewModel` facade 已移除。

完整旧新索引见 [Legacy 能力最终映射](legacy-capability-map.md)。

## 部分完成

| 能力 | 已完成 | 剩余工作 |
|---|---|---|
| Manual 跟踪 | 手动选点、人工 blob、AE/TWDW 输出、相邻帧速度、UI/处理/存储闭环 | 对照旧版补齐 `MANUAL_Assoc3`、`MANUAL_VerifyTarget`、`MANUAL_TrackTarget` 的完整历史校验语义 |
| 图像处理 | Direct、OpenCV、Diff、CUDA 可选策略及参数配置 | 光度、星图、完整暗场/平场定标、TWDW 和指向误差处理 |
| 串口运行诊断 | 帧长/头尾、BCD 时间和主控时间窗口错误事件已进入日志 | 断连重连、统计聚合和更多协议字段约束 |
| 相机串口 | 命令编码与 `SerialCameraController` 发送适配器已有测试 | 默认组合根仍使用 `CommandOnlyCameraController`；需注入具体 `ICameraSerialPort` |
| Sapera 采集 | 可选 SDK 适配器、RAII 会话、错误事件、契约测试和 smoke 入口 | 在真实采集机回填 smoke 结果 |
| CUDA 加速 | 可选处理策略、无 CUDA 安全退化、固定帧对照测试和基准入口 | 在 CUDA 设备验证输出容差与 6144 性能；达到收益门槛后再加入 UI |

Sapera/CUDA 的命令、门槛和结果回填表见 [硬件验证](hardware-validation.md)。

## 未迁移

| Legacy 能力 | 状态 | 决策 |
|---|---|---|
| `SingleApplication` | 未迁移 | 低优先级；需要单实例产品需求时再实现 |
| `ENetServer` | 未迁移 | 低优先级；当前 UDP 服务不依赖 ENet |
| `UI_DistCurve` | 未迁移 | 中优先级；分析页仍保留距离曲线入口空间 |

## 当前验收基线

- 默认无 Sapera、无 CUDA 的 `clang-cl-debug` 构建：218/218 CTest 通过。
- Sapera 和 CUDA 都是显式启用能力，默认配置必须继续保持无硬件安全。
- 硬件验收未执行前，不把 Sapera 标记为“硬件验证完成”，也不在 UI 暴露 CUDA 模式。

## 模块文档

- [Core](module-core.md)
- [Application](module-app.md)
- [Acquisition](module-acquisition.md)
- [Processing](module-processing.md)
- [Tracking](module-tracking.md)
- [Storage](module-storage.md)
- [Communication](module-comm.md)
- [Network](module-network.md)
- [GPU](module-gpu.md)
- [UI](module-ui.md)
