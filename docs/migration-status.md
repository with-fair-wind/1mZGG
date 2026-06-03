# oldsrc → 新架构迁移进度

> 本文档跟踪从旧版 qmake/OpenCL 单体代码 (`oldsrc/`) 到新版 CMake 模块化架构的迁移状态。
>
> 最近更新: 2026-06-03。当前策略仍是 `oldsrc/` 只读参考，不纳入新 CMake 构建。

## 迁移概览

| 类别 | 文件数 | 已完成 | 部分完成 | 未开始 |
|------|--------|--------|---------|--------|
| 核心/基础设施 | 7 | 7 | 0 | 0 |
| 串口通信 | 4 | 4 | 0 | 0 |
| 网络通信 | 6 | 5 | 0 | 1 |
| 图像处理 | 4 | 1 | 2 | 1 |
| 跟踪算法 | 4 | 1 | 3 | 0 |
| 存储 | 3 | 1 | 2 | 0 |
| 采集/相机 | 2 | 1 | 1 | 0 |
| GPU | 2 | 2 | 0 | 0 |
| UI | 6 | 2 | 2 | 2 |
| **合计** | **38** | **24** | **10** | **4** |

**整体进度: 约 63% 已完成，26% 部分完成，11% 未开始**

当前进度判断:
- 协议与服务链路第一批已落地: 四路串口、图像发送、心跳、诊断、数据交换、大气接收、存储后端和相机命令控制器都能通过 `ApplicationContext::registerCommunicationServices()` 注册到 `ServiceRegistry`。
- 默认启动策略保持无硬件安全: 服务注册后 `isOpen() == false`，不会自动打开串口或绑定 UDP 端口。
- 旧数学基础能力已迁入 `Dss::Math`: `mpolyfit`、`getperiod` 和 `mFFT::FFT_process` 的可观察行为已通过纯 C++ helper 与单元测试覆盖。
- 图像序列回放首版已落地: `ImageSequenceFrameSource` 可选择 BMP/PNG/JPEG/TIFF/raw 序列，按 `IFrameSource` 回调输出 `FramePacket`，并通过 `ImageProcessor` 进入 `DisplayRefreshEvent` 和 UI 显示。
- 本地图像存储 raw 异步写入 worker 已落地: UI 保存开关显式启动 `LocalImageStorageBackend`，回放帧进入处理器前可异步落盘，停止时 drain 队列。
- 当前最大剩余面是 Manual/GEO 跟踪算法、完整图像处理策略、Sapera 真实采集器、LEO/SC 跟踪和 CUDA 管线封装；Sapera 已不是无硬件端到端开发的前置条件。

---

## 详细迁移状态

### 已完成迁移 (Completed)

以下文件/功能已完全迁移到新架构并通过测试。

| 旧版文件 | 新版位置 | 说明 |
|---------|---------|------|
| `eventBus17.hpp` | `include/dss/core/event_bus.h` | 事件总线，增强版 (COW、锁策略、ScopedConnection) |
| `GlobalParameter.h/.cpp` (类型部分) | `include/dss/core/types.h` / `config_types.h` | 领域类型拆分为独立结构体 |
| `GlobalParameter.h/.cpp` (配置部分) | `include/dss/core/config.h` + `src/core/config.cpp` | QSettings/INI → JSON |
| `DefinedMacro.h` | `include/dss/core/constants.h` | 宏 → enum class + constexpr |
| `MyLog.h/.cpp` | `include/dss/core/logger.h` | 事件总线驱动的日志 |
| `StandardThread.h/.cpp` | `std::jthread` (各模块内使用) | 标准库替代自定义线程 |
| `CommDisplay.h/.cpp` | `dss/comm/display_channel.*` + `serial_protocol_codec.h` | 协议编解码完整 |
| `CommExposure.h/.cpp` | `dss/comm/exposure_channel.*` + `serial_protocol_codec.h` | 协议编解码完整 |
| `CommMasterControl.h/.cpp` | `dss/comm/master_control_channel.*` + `serial_protocol_codec.h` | 协议编解码完整 |
| `CommServo.h/.cpp` | `dss/comm/servo_channel.*` + `serial_protocol_codec.h` | 协议编解码完整 |
| `CommCamera.h/.cpp` (命令编码) | `dss/acquisition/camera_control_protocol.h` | 3 字节寄存器命令完整 |
| `NetImageSender.h/.cpp` | `dss/network/image_sender.h` | 分片发送 + 工作线程 |
| `NetAtmos.h/.cpp` | `dss/network/atmos_receiver.h` + `atmos_protocol.h` | 接收解码完整 |
| `NetExchange.h/.cpp` | `dss/network/data_exchange.h` + `data_exchange_protocol.h` | GXTC/GDCL 编码完整 |
| `NetErrorDiagnose.h/.cpp` | `dss/network/error_diagnostics.h` + `diagnostic_protocol.h` | JSON 诊断完整 |
| `NetApp.h/.cpp` (心跳部分) | `dss/network/heartbeat.h` | 心跳 + 关闭保护帧 |
| `Labeler.h/.cpp` | `dss/processing/labeler.h` + `src/processing/labeler.cpp` | CPU 连通域检测 |
| `ImageCode.h/.cpp` (格式定义) | `dss/storage/image_storage_format.h` / `bmp_image_format.h` | RAW/BMP 格式头 |
| `TrackDataStorage.h/.cpp` (格式) | `dss/storage/track_data_storage_format.h` | 文本行格式 |
| `mpolyfit.h/.cpp` | `dss/tracking/math_utils.h/.cpp` | 多项式拟合 |
| `mfft.h/.cpp` | `dss/tracking/math_utils.h/.cpp` | FFT/legacy 频谱: 补零、幅值归一、相位、基频 |
| `getperiod.h/.cpp` | `dss/tracking/math_utils.h/.cpp` | 周期估计 |
| `DeviceManager.h/.cpp` | `dss/gpu/cuda_device_manager.h/.cpp` | OpenCL→CUDA 设备管理 |
| `KernelCL.cl` | `kernels/*.cu` (6 个 CUDA 文件) | OpenCL→CUDA 核函数 |

### 部分完成 (In Progress)

以下文件的结构和接口已建立，但核心算法/逻辑尚未移植。

| 旧版文件 | 新版位置 | 已完成 | 待完成 |
|---------|---------|--------|--------|
| `TrackAlgo.h/.cpp` (GEO) | `dss/tracking/geo_tracker.*` | 类骨架、方法签名 | **所有核心算法** (calcStarSpeed, assoc4, findTargets, trackTargets) |
| `TrackAlgo.h/.cpp` (LEO) | `dss/tracking/leo_tracker.*` | 类骨架 | **track() 算法体** |
| `TrackAlgo.h/.cpp` (SC) | `dss/tracking/sc_tracker.*` | 类骨架 | **track() 算法体** |
| `TrackAlgo.h/.cpp` (Manual) | `dss/tracking/manual_tracker.*` | 类骨架、setManualTarget | **track() 算法体** |
| `ImageProcessor.h/.cpp` | `dss/processing/image_processor.*` | 工作线程、帧队列、事件发布、ApplicationContext 注册 | GPU/光度/星图集成 |
| `ImageProcAlgo.h/.cpp` | `OpenCvProcessingStrategy` + CUDA kernels | OpenCV 参考实现 | 帧差法、定标、光度、TWDW、指向误差 |
| `ImageStorage.h/.cpp` (I/O) | `LocalImageStorageBackend` | 格式定义、raw 异步写入 worker | BMP/IFM/会话索引、错误上报 |
| `TrackDataStorage.h/.cpp` (I/O) | `TrackDataStorageBackend` | 格式定义、路径持有 | 文件 I/O 操作 |
| `ImageReplayer.h/.cpp` | `ImageSequenceFrameSource` | 选择序列、QImage/raw 解码、后台回放、接入处理/显示 | 暂停续播进度、当前帧进度、更多 legacy 浏览行为 |
| `UI_CtrlPad.h/.cpp/.ui` | `dss/ui/main_window.*` + `view_model.*` | 选择序列、开始/暂停回放、保存、跟踪模式 UI 首版 | 当前帧进度、处理策略开关、硬件命令入口 |

### 未开始 (Not Started)

| 旧版文件 | 说明 | 优先级 |
|---------|------|--------|
| `Grabber.h/.cpp` | Sapera SDK 帧采集器 (~500行) | 中 — 真实硬件采集入口，回放模式可先绕过 |
| `SingleApplication.h/.cpp` | 单实例保护 (QLocalServer) | 低 |
| `ENetServer.h` | 可靠 UDP 传输 (ENet) | 低 |
| `UI_DistCurve.h/.cpp/.ui` | 距离曲线图表 | 中 |

---

## 架构变更记录

| 旧版模式 | 新版模式 | 原因 |
|---------|---------|------|
| qmake (`.pro`) | CMake + CMakePresets | 现代构建系统，跨平台 |
| 单体编译 | 9 个静态库 target | 模块化，独立测试 |
| OpenCL | CUDA | GPU 生态更成熟 |
| QSettings/INI | nlohmann_json/JSON | 结构化配置 |
| `#define` 宏 | `enum class` + `constexpr` | 类型安全 |
| `GlobalParameter` 全局单例 | `Config` + 分散 DTO | 松耦合 |
| 自定义线程 (`StandardThread`) | `std::jthread` | 协作式取消 |
| 直接函数调用 | `BasicMessageBus` 事件总线 | 解耦 |
| UI 直接持有子系统 | MVVM + ServiceRegistry | 关注点分离 |
| 原始指针串口 | pimpl + RAII | 安全性 |
| UI 直接启动硬件 | 服务注册但默认不打开端口 | 无硬件环境可启动、便于测试 |

---

## 六阶段迁移路线图

| 阶段 | 内容 | 状态 |
|------|------|------|
| **Phase 1** | 项目骨架: CMake、Core 类型/事件/配置、事件总线 | **已完成** |
| **Phase 2** | 通信层: 串口协议编解码、通道实现、网络服务 | **已完成**，服务已注册但默认不开硬件端口 |
| **Phase 3** | 处理管线: FramePacket、Pipeline、Labeler、OpenCV 后端 | **已完成** |
| **Phase 4** | 跟踪算法: TrackManager 骨架、数学工具、Tracker 接口 | **数学工具完成，策略算法待移植** |
| **Phase 5** | GPU 后端: CUDA 设备管理、核函数移植 | **核函数完成，管线集成待做** |
| **Phase 6** | UI 集成: MainWindow、ViewModel、端到端接线 | **回放/保存首版已接线，硬件/策略命令待做** |

### 后续迁移计划

| 顺序 | 迁移块 | 目标 | 验证重点 |
|------|--------|------|----------|
| 1 | 回放模式帧源 | 迁移 `ImageReplayer` 思路，支持选择图像序列并作为 `IFrameSource` 推送帧 | **首版完成**：`test_image_sequence_frame_source` 覆盖固定序列 |
| 2 | 回放端到端处理链路 | 将回放帧接入 `ImageProcessor`/`ProcessingPipeline`/显示事件 | **首版完成**：无相机可驱动 UI 显示，6144 大图显示/滚轮缩放已在 `ImageDisplay` 支持 |
| 3 | 存储 I/O 工作线程 | 将 `LocalImageStorageBackend` 从格式 helper 推进到实际异步写入 | **部分完成**：raw worker 和 drain 测试完成；BMP/IFM/轨迹文本待补 |
| 4 | UI 回放/存储/处理命令 | 搭起选择序列、开始/暂停回放、保存、处理、跟踪等显式命令 | **部分完成**：选择序列、开始/暂停、保存、跟踪模式已接；处理策略开关/进度待补 |
| 5 | Manual 跟踪最小策略 | 先迁移最简单的手动目标保持逻辑，打通 tracking event | 固定输入目标列表的确定性测试 |
| 6 | GEO 跟踪策略 | 逐步迁移 `calcStarSpeed`、`assoc4`、`findTargets`、`trackTargets` | 以旧算法样例/合成帧做回归测试 |
| 7 | Sapera 采集器 | 回放链路稳定后接真实 Sapera `Grabber` 为另一个 `IFrameSource` | 无 Sapera 时仍可启动；有硬件时显式打开 |
| 8 | LEO/SC 跟踪策略 | 在 Manual/GEO 稳定后迁移剩余跟踪模式 | 与 GEO 共用测量 DTO 和回归样例 |
| 9 | CUDA 管线封装 | 把 CUDA kernel 包装为 `IProcessingStrategy` 并接入 `ProcessingPipeline` | CPU/OpenCV 对照、CUDA 可选启用 |

---

## 风险与建议

### 高风险项

1. **跟踪算法移植** — `TrackAlgo.cpp` 约 3000 行核心算法，是最大的迁移任务块。建议逐模式移植：先 Manual 打通用户选点与 tracking event，再 GEO，最后 LEO/SC。

2. **处理策略补齐** — 回放源已能驱动 `ImageProcessor` 原样显示，下一步需要把 legacy 图像处理/目标检测策略接入 `ProcessingPipeline`，为 Manual/GEO 提供测量输入。

3. **硬件入口接线** — 当前通信/网络/存储/相机命令服务已注册，但启动策略仍是默认不打开硬件。后续需要 UI 或联调入口显式触发 open/bind/start，并把 Sapera 作为第二个 `IFrameSource` 接入。

### 中风险项

4. **GPU 管线集成** — CUDA 核函数已就绪但无 `IProcessingStrategy` 封装，无法通过 `ProcessingPipeline` 调用。

5. **完整存储会话** — raw worker 已有，仍需补 BMP/IFM/IMI、轨迹文本、错误上报和高帧率背压策略。
