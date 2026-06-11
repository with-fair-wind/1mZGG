# oldsrc → 新架构迁移进度

> 本文档跟踪从旧版 qmake/OpenCL 单体代码 (`oldsrc/`) 到新版 CMake 模块化架构的迁移状态。
>
> 最近更新: 2026-06-07。当前策略仍是 `oldsrc/` 只读参考，不纳入新 CMake 构建。

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
- 本地存储首批 worker 已落地: UI 保存开关显式启动 `LocalImageStorageBackend` 和 `TrackDataStorageBackend`，回放帧进入处理器前可异步落 raw，tracking event 可异步写入 `track_data.txt`，停止时 drain 队列。
- Manual 跟踪最小闭环已落地: UI 点击图像像素后会配置 `ManualTracker`，无处理 backend 的回放帧也能进入 tracking，发布 `TrackResultEvent`，更新 UI 跟踪信息，并在保存开启时写入轨迹文本。
- GEO 跟踪函数级切片继续推进: 星速估计、四帧初始关联、目标发现标记、基础跟踪维持、同帧测量复用抑制、预测越界结束、连续无效窗口结束目标、连续有效测量落在同一赤道坐标点时结束目标、FullLEO 像素跟踪自适应搜索半径和速度误差门控、非 FullLEO RA/Dec `Assoc4` 初始关联、非 FullLEO RA/Dec TrackTarget 首片、可选 AE 位置阈值 gate 首片、目标全部失活后重新进入四帧关联/重发现状态切换、已有目标存活时追加新四帧候选并按当前测量空间过滤最近四帧重叠轨迹（FullLEO 质心/非 FullLEO RA/Dec）、会话内 GEO 目标唯一 ID 分配，以及 legacy 外部校验 blob 输入到 invalid frame fallback 并按目标 ID 匹配的首批接线，已有纯函数/策略实现与单元测试；外部校验 fallback 消费端已公共化，外部校验 blob 生成、TWDW/GDCL 仍待继续迁移。
- Tracking 公共候选/预测/匹配与生命周期 helper 已抽取: `candidate_utils` 提供按初始帧测量复用的候选去重 policy，以及按最近帧窗口判断 living target 重叠的 `RecentMeasurementOverlapRule`；`prediction_utils` 提供 `TargetFrameInfo` 构造、invalid fallback、外部校验 fallback、Frame/AE 运动量、最近 blob 匹配、匹配/invalid 帧追加和最近四帧 median 预测更新；`lifecycle_utils` 提供最近 invalid 计数、连续 invalid 判断/释放策略、latest valid、validity window、有效率更新和 `TrackLivingRule`/`TrackMissPolicy` 策略入口；GEO/LEO/SC 复用这些纯函数，策略类继续只保留各自的候选生成、门限和 legacy living policy 选择。
- LEO 跟踪继续推进: `LeoTracker::track()` 现在维护三帧 FIFO，并按 legacy `LEO_Assoc3` 的 AE 速度下限和两段 AE 运动一致性生成初始候选；第四帧可按预测 AE 位置匹配 blob，追加验证帧，用最近三段运动 median 更新预测，并选出已验证目标；第四帧未命中时会追加 invalid 帧、丢弃未验证候选，并允许后续帧重新进入三帧关联完成重发现；验证后的持续跟踪已开始接入，第五帧有效测量会按预测 AE 匹配、追加并推进下一帧预测，后续单帧 miss 会按预测位置追加 invalid 帧并保持目标存活；`test_leo_tracker` 覆盖命中、低速拒绝、第四帧验证、第五帧持续跟踪、单帧跟踪 miss 和验证失败后重发现。
- SC 跟踪继续推进: `ScTracker::track()` 现在维护三帧 FIFO，并按 legacy `SC_Assoc3` 的像素位移半径、FOV 中心窗口和两段像素运动一致性生成初始候选；三帧候选会按 oldsrc 的原始候选全集比较压缩复用初始测量点的相似轨迹，链式重复场景已覆盖；第四帧可按预测像素位置验证，验证分支使用 validity-window living policy，验证后可按预测像素位置和 FOV 中心窗口持续跟踪，TrackTarget 分支使用 legacy 活动逻辑中的 latest-valid 释放策略，miss 后允许后续重新三帧关联；`test_sc_tracker`、`test_tracking_candidate_utils` 和 `test_tracking_lifecycle_utils` 覆盖三帧候选、重复候选压缩、第四帧验证、第五帧持续跟踪、miss 后重发现和 living policy 选择。
- 当前最大剩余面是 GEO 完整策略、Manual legacy 三帧关联细节、LEO/SC 的 TWDW/GDCL 分支、完整图像处理策略、Sapera 真实采集器和 CUDA 管线封装；Sapera 已不是无硬件端到端开发的前置条件。

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
| `TrackAlgo.h/.cpp` (GEO) | `dss/tracking/geo_tracker.*` + `candidate_utils.*` + `prediction_utils.*` + `lifecycle_utils.*` | `calcStarSpeed` 纯 helper、星速 AE 换算、四帧初始关联首版、`findTargets`/`trackTargets` 基础维持、同帧测量复用抑制、预测越界/连续无效窗口 living 规则、连续有效测量重复赤道坐标点结束规则、FullLEO 像素跟踪自适应搜索半径和速度误差门控、非 FullLEO RA/Dec `Assoc4` 初始关联和 TrackTarget 首片、可选 AE 位置阈值 gate 首片、外部校验 blob 输入到 invalid frame fallback 并按目标 ID 匹配、目标丢失后重新进入四帧关联、已有目标存活时追加新四帧候选并按当前测量空间执行最近四帧 living-target 重叠去重、会话内 GEO 目标唯一 ID 分配、公共候选去重/最近帧重叠、外部校验 fallback 和生命周期 helper、`test_geo_tracker` 覆盖 | 外部校验 blob 生成、TWDW/GDCL |
| `TrackAlgo.h/.cpp` (LEO) | `dss/tracking/leo_tracker.*` + `prediction_utils.*` + `lifecycle_utils.*` | 类骨架、三帧 FIFO、`LEO_Assoc3` 初始关联首片、AE 速度下限/两段 AE 运动一致性 gate、三帧 `TargetInfo` 预测输出、`LEO_VerifyTarget` 第四帧匹配验证首片、第四帧 invalid fallback 和验证失败后重发现首片、`LEO_TrackTarget` 有效测量持续跟踪首片、单帧 miss invalid fallback 首片、连续 5 帧 miss 释放目标、最近三段运动 median 预测更新、最快 AE 目标选择、LEO/SC 共享预测与匹配 helper、公共生命周期 helper、`test_leo_tracker` 覆盖 | TWDW/GDCL |
| `TrackAlgo.h/.cpp` (SC) | `dss/tracking/sc_tracker.*` + `candidate_utils.*` + `prediction_utils.*` + `lifecycle_utils.*` | 类骨架、三帧 FIFO、`SC_Assoc3` 初始关联首片、像素位移半径/FOV 中心窗口/两段像素运动一致性 gate、按 oldsrc 原始候选全集比较的复用初始测量点重复候选压缩、三帧 `TargetInfo` 预测输出、`SC_VerifyTarget` 第四帧预测像素匹配首片、`SC_VerifyTarget` validity-window living policy、`SC_TrackTarget` 第五帧持续跟踪/latest-valid 释放策略/miss 后重发现首片、LEO/SC 共享预测与匹配 helper、公共候选去重和生命周期 policy helper、`test_sc_tracker`/`test_tracking_candidate_utils`/`test_tracking_lifecycle_utils` 覆盖 | TWDW/GDCL |
| `TrackAlgo.h/.cpp` (Manual) | `dss/tracking/manual_tracker.*` | 手动选点人工 blob、AE 解算、TargetInfo 输出、UI/ImageProcessor 闭环 | legacy 三帧关联/Verify/TrackTarget 细节、TWDW/GDCL |
| `ImageProcessor.h/.cpp` | `dss/processing/image_processor.*` | 工作线程、帧队列、事件发布、ApplicationContext 注册、Direct/Manual 无 backend 跟踪 | GPU/光度/星图集成 |
| `ImageProcAlgo.h/.cpp` | `OpenCvProcessingStrategy` + CUDA kernels | OpenCV 参考实现 | 帧差法、定标、光度、TWDW、指向误差 |
| `ImageStorage.h/.cpp` (I/O) | `LocalImageStorageBackend` | 格式定义、raw 异步写入 worker | BMP/IFM/会话索引、错误上报 |
| `TrackDataStorage.h/.cpp` (I/O) | `TrackDataStorageBackend` | 格式定义、路径持有、`track_data.txt` 异步写入、`TrackResultEvent` 接线 | GAE/会话级轨迹文件、错误上报、高帧率背压 |
| `ImageReplayer.h/.cpp` | `ImageSequenceFrameSource` | 选择序列、QImage/raw 解码、后台回放、保留下一帧索引、单帧前进、接入处理/显示 | 后退、进度定位、更多 legacy 浏览行为 |
| `UI_CtrlPad.h/.cpp/.ui` | `dss/ui/main_window.*` + `view_model.*` | 选择序列、开始/暂停回放、当前帧进度、单帧前进、保存、None/OpenCV 处理开关、Manual 选点跟踪 UI 首版、GEO/LEO/SC 策略入口 | 进度条/后退、Diff/CUDA/参数化处理策略、硬件命令入口、LEO/SC 算法体 |

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
| **Phase 4** | 跟踪算法: TrackManager 骨架、数学工具、Tracker 接口 | **数学工具完成，Manual 最小闭环完成，GEO 第一批函数级切片完成，LEO Assoc3/VerifyTarget/TrackTarget 首片完成，SC Assoc3/VerifyTarget/TrackTarget 首片完成** |
| **Phase 5** | GPU 后端: CUDA 设备管理、核函数移植 | **核函数完成，管线集成待做** |
| **Phase 6** | UI 集成: MainWindow、ViewModel、端到端接线 | **回放/保存/Manual 选点跟踪首版已接线，硬件/策略命令待做** |

### 后续迁移计划

| 顺序 | 迁移块 | 目标 | 验证重点 |
|------|--------|------|----------|
| 1 | 回放模式帧源 | 迁移 `ImageReplayer` 思路，支持选择图像序列并作为 `IFrameSource` 推送帧 | **首版完成**：`test_image_sequence_frame_source` 覆盖固定序列 |
| 2 | 回放端到端处理链路 | 将回放帧接入 `ImageProcessor`/`ProcessingPipeline`/显示事件 | **首版完成**：无相机可驱动 UI 显示，6144 大图显示/滚轮缩放已在 `ImageDisplay` 支持 |
| 3 | 存储 I/O 工作线程 | 将存储后端从格式 helper 推进到实际异步写入 | **部分完成**：raw worker、轨迹文本 worker 和 drain 测试完成；BMP/IFM/IMI/GAE/错误上报/背压待补 |
| 4 | UI 回放/存储/处理命令 | 搭起选择序列、开始/暂停回放、保存、处理、跟踪等显式命令 | **部分完成**：选择序列、开始/暂停、当前帧进度、单帧前进、保存、None/OpenCV 处理开关、跟踪模式已接；进度条/后退、Diff/CUDA/参数化处理待补 |
| 5 | Manual 跟踪最小策略 | 先迁移最简单的手动目标保持逻辑，打通 tracking event 和轨迹文本写入 | **首版完成**：`test_manual_tracker`、`test_image_processor`、`test_view_model_tracking`、`test_track_data_storage_backend` 覆盖无 backend 回放闭环和保存开关 |
| 6 | GEO 跟踪策略 | 逐步迁移 `calcStarSpeed`、`assoc4`、`findTargets`、`trackTargets` | **继续推进**：星速估计、四帧关联、基础维持、测量复用抑制、越界/连续无效结束、连续有效测量重复赤道坐标点结束、FullLEO 像素跟踪自适应半径/速度误差门控、非 FullLEO RA/Dec `Assoc4` 初始关联和 TrackTarget 首片、可选 AE 位置阈值 gate、外部校验 blob 输入到 invalid frame fallback 并按目标 ID 匹配、外部校验 fallback 消费端公共化、会话内唯一目标 ID、丢失后重发现状态切换、已有目标存活时追加新四帧候选及按当前测量空间的重叠去重已由 `test_geo_tracker`/`test_tracking_candidate_utils`/`test_tracking_prediction_utils` 覆盖；下一步补外部校验 blob 生成和 TWDW/GDCL |
| 7 | Sapera 采集器 | 回放链路稳定后接真实 Sapera `Grabber` 为另一个 `IFrameSource` | 无 Sapera 时仍可启动；有硬件时显式打开 |
| 8 | LEO/SC 跟踪策略 | 在 Manual/GEO 稳定后迁移剩余跟踪模式 | **LEO/SC 继续推进**：LEO 三帧初始关联、低速拒绝、第四帧验证、第五帧持续跟踪、单帧跟踪 miss、连续 5 帧 miss 释放目标和验证失败后重发现已由 `test_leo_tracker` 覆盖；SC 三帧初始关联、重复候选压缩、第四帧验证、第五帧持续跟踪、living policy 选择和 miss 后重发现已由 `test_sc_tracker`/`test_tracking_candidate_utils`/`test_tracking_lifecycle_utils` 覆盖；两者已复用 `prediction_utils` 的帧信息构造、invalid fallback、Frame/AE 匹配、匹配/invalid 帧追加和 median 预测更新，并复用 `candidate_utils` 的候选去重与 `lifecycle_utils` 的生命周期判断/policy 入口；后续补 TWDW/GDCL |
| 9 | CUDA 管线封装 | 把 CUDA kernel 包装为 `IProcessingStrategy` 并接入 `ProcessingPipeline` | CPU/OpenCV 对照、CUDA 可选启用 |

---

## 风险与建议

### 高风险项

1. **跟踪算法移植** — `TrackAlgo.cpp` 约 3000 行核心算法，是最大的迁移任务块。Manual 最小闭环和 GEO 第一批函数级切片已打通，目标丢失后也能回到四帧关联入口，已有目标存活时也能追加新四帧候选并按当前测量空间过滤重叠轨迹，连续有效测量落在同一赤道坐标点会按 legacy 规则结束目标，FullLEO 像素跟踪已补入最近无效帧自适应搜索半径和速度误差门控，非 FullLEO RA/Dec `Assoc4` 初始关联、TrackTarget 首片和重发现 living-target 重叠去重已接入，可选 AE 位置阈值 gate 已开始接入，外部校验 blob 也已能进入 GEO invalid frame fallback 并按会话内唯一目标 ID 匹配，fallback 消费端已抽为公共 helper；LEO 已完成三帧初始关联、第四帧匹配验证、第五帧持续跟踪、单帧跟踪 miss、连续 5 帧 miss 释放目标和验证失败后重发现首片；SC 已完成三帧初始关联、重复候选压缩、第四帧验证、第五帧持续跟踪、verify/track living policy 选择和 miss 后重发现首片；共享候选去重、最近帧重叠、预测、匹配、外部校验 fallback、帧追加和生命周期 policy helper 已抽取，后续新增策略分支可减少重复逻辑。下一步继续补 GEO 的外部校验 blob 生成，以及 LEO/SC 的 TWDW/GDCL。

2. **处理策略补齐** — 回放源已能驱动 `ImageProcessor` 原样显示，UI 已可切 None/OpenCV；下一步需要继续把 legacy 帧差法、参数化阈值、光度/星图能力接入 `ProcessingPipeline`，为 Manual/GEO 提供更完整测量输入。

3. **硬件入口接线** — 当前通信/网络/存储/相机命令服务已注册，但启动策略仍是默认不打开硬件。后续需要 UI 或联调入口显式触发 open/bind/start，并把 Sapera 作为第二个 `IFrameSource` 接入。

### 中风险项

4. **GPU 管线集成** — CUDA 核函数已就绪但无 `IProcessingStrategy` 封装，无法通过 `ProcessingPipeline` 调用。

5. **完整存储会话** — raw worker 和轨迹文本 worker 已有，仍需补 BMP/IFM/IMI、GAE/会话索引、错误上报和高帧率背压策略。
