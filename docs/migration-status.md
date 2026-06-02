# oldsrc → 新架构迁移进度

> 本文档跟踪从旧版 qmake/OpenCL 单体代码 (`oldsrc/`) 到新版 CMake 模块化架构的迁移状态。

## 迁移概览

| 类别 | 文件数 | 已完成 | 部分完成 | 未开始 |
|------|--------|--------|---------|--------|
| 核心/基础设施 | 7 | 7 | 0 | 0 |
| 串口通信 | 4 | 4 | 0 | 0 |
| 网络通信 | 6 | 5 | 0 | 1 |
| 图像处理 | 4 | 1 | 2 | 1 |
| 跟踪算法 | 4 | 1 | 3 | 0 |
| 存储 | 3 | 1 | 2 | 0 |
| 采集/相机 | 2 | 1 | 0 | 1 |
| GPU | 2 | 2 | 0 | 0 |
| UI | 6 | 2 | 2 | 2 |
| **合计** | **38** | **24** | **9** | **5** |

**整体进度: 约 63% 已完成，24% 部分完成，13% 未开始**

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
| `mfft.h/.cpp` | `dss/tracking/math_utils.h/.cpp` | FFT |
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
| `ImageProcessor.h/.cpp` | `dss/processing/image_processor.*` | 工作线程、帧队列、事件发布 | GPU/光度/星图集成、注册到 ApplicationContext |
| `ImageProcAlgo.h/.cpp` | `OpenCvProcessingStrategy` + CUDA kernels | OpenCV 参考实现 | 帧差法、定标、光度、TWDW、指向误差 |
| `ImageStorage.h/.cpp` (I/O) | `LocalImageStorageBackend` | 格式定义、路径持有 | 异步写入工作线程 |
| `TrackDataStorage.h/.cpp` (I/O) | `TrackDataStorageBackend` | 格式定义、路径持有 | 文件 I/O 操作 |
| `UI_CtrlPad.h/.cpp/.ui` | `dss/ui/main_window.*` + `view_model.*` | MVVM 骨架、页面结构 | 控制逻辑接线 (grab/track/save) |

### 未开始 (Not Started)

| 旧版文件 | 说明 | 优先级 |
|---------|------|--------|
| `Grabber.h/.cpp` | Sapera SDK 帧采集器 (~500行) | **高** — 无此项系统无法运行 |
| `SingleApplication.h/.cpp` | 单实例保护 (QLocalServer) | 低 |
| `ENetServer.h` | 可靠 UDP 传输 (ENet) | 低 |
| `ImageReplayer.h/.cpp` | 图像回放/浏览 | 中 |
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

---

## 六阶段迁移路线图

| 阶段 | 内容 | 状态 |
|------|------|------|
| **Phase 1** | 项目骨架: CMake、Core 类型/事件/配置、事件总线 | **已完成** |
| **Phase 2** | 通信层: 串口协议编解码、通道实现、网络服务 | **已完成** |
| **Phase 3** | 处理管线: FramePacket、Pipeline、Labeler、OpenCV 后端 | **已完成** |
| **Phase 4** | 跟踪算法: TrackManager 骨架、数学工具、Tracker 接口 | **结构完成，算法待移植** |
| **Phase 5** | GPU 后端: CUDA 设备管理、核函数移植 | **核函数完成，管线集成待做** |
| **Phase 6** | UI 集成: MainWindow、ViewModel、端到端接线 | **骨架完成，接线待做** |

---

## 风险与建议

### 高风险项

1. **跟踪算法移植** — `TrackAlgo.cpp` 约 3000 行核心算法，是最大的迁移任务块。建议逐模式移植：先 GEO（主要使用场景），再 Manual（最简单），最后 LEO/SC。

2. **Grabber 迁移** — 无实际帧源则系统无法端到端运行。建议先实现模拟帧源 (`SimulatedFrameSource`) 用于开发和测试。

3. **端到端接线** — 当前各模块独立工作，但 `main.cpp` 未调用 `startServices()`，`ImageProcessor` 未注册。需要完成 Phase 6 的服务编排。

### 中风险项

4. **GPU 管线集成** — CUDA 核函数已就绪但无 `IProcessingStrategy` 封装，无法通过 `ProcessingPipeline` 调用。

5. **存储异步写入** — 格式定义完整但无写入工作线程，高帧率下可能成为瓶颈。
