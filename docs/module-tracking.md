# Tracking 模块 (`dss_tracking`)

> 命名空间: `Dss::Tracking`、`Dss::Math`
>
> 头文件: `include/dss/tracking/`
>
> 源文件: `src/tracking/`
>
> 依赖: `dss_core`

## 模块职责

Tracking 模块实现天文目标跟踪算法，支持 GEO（地球静止轨道）、LEO（低地球轨道）、SC（星表）和 Manual（手动）四种跟踪模式。通过策略模式允许运行时切换。

## 组件清单

### 1. ITrackingStrategy (`i_tracking_strategy.h`)

跟踪策略接口：

```cpp
class ITrackingStrategy {
    virtual auto track(const FrameMeasurements& measurements)
        -> vector<TargetInfo> = 0;
    virtual auto mode() const -> TrackMode = 0;
    virtual void reset() = 0;
};
```

### 2. TrackManager (`track_manager.h`)

跟踪管理器，持有当前策略实例，线程安全（互斥锁）。

| 方法 | 说明 |
|------|------|
| `setStrategy(strategy)` | 替换跟踪策略 |
| `setMode(mode, settings)` | 按模式创建并切换 GEO/Manual/LEO/SC 策略，`Init` 清空当前策略 |
| `track(measurements)` | 委托当前策略执行跟踪 |
| `currentMode()` | 获取当前跟踪模式 |
| `reset()` | 重置策略状态 |

### 公共候选/预测/匹配与生命周期工具

LEO/SC/GEO 迁移中抽出的 Qt-free 纯函数层，用于复用候选去重、目标帧信息构造、预测 fallback、blob 匹配和基础生命周期判断。策略模式仍由 `ITrackingStrategy`、`GeoTracker`、`LeoTracker`、`ScTracker` 等类承担；公共 helper 只提供可组合的算法积木，具体候选生成、门限和 legacy policy 选择仍保留在各策略中。

| 函数/类型 | 说明 |
|-----------|------|
| `candidate_utils.h` | 按初始帧同 index 测量复用进行候选去重，保留 oldsrc 基于原始候选全集比较、保留首个候选的压缩语义 |
| `prediction_utils.h` | 目标帧信息、预测、运动量和最近 blob 匹配 helper |
| `makeTargetFrameInfo()` / `makeInvalidTargetFrameInfo()` | 构造有效测量帧和预测 invalid 帧 |
| `frameMotion()` / `aeMotion()` / `target*MotionAt()` | 计算像素空间和 AE 空间运动量 |
| `findNearestBlob(frame, target, settings, options)` | 按 `BlobMatchSpace::Frame` 或 `BlobMatchSpace::Ae` 查找最近候选，可选中心视场过滤 |
| `updatePredictionFromRecentFour()` | 使用最近三段运动的 median 更新下一帧预测和有效率 |
| `lifecycle_utils.h` | 最近 invalid 计数、连续 invalid 判断、latest valid、validity window、有效率更新和 `TrackLivingRule`/`TrackMissPolicy` 生命周期策略入口 |

### 3. GeoTracker (`geo_tracker.h`)

GEO 目标跟踪器，对应旧版 `TrackAlgo` 中 GEO 模式的算法。

**旧版核心算法:**
- `calcStarSpeed()` — 星体运动速度计算
- `assoc4()` — 四帧关联
- `findTargets()` — 新目标发现
- `trackTargets()` — 已知目标跟踪维持

**当前状态:** 第一批函数级切片已完成并继续补齐。`estimateGeoStarSpeed()` 提供 Qt-free 纯函数用于估计中心视场星速和 AE 速度；`calcStarSpeed()` 已接入策略状态；`assoc4()` 已迁移确定性的四帧候选关联、星速过滤、运动一致性过滤和基础重复抑制；`findTargets()`/`trackTargets()` 已能发布基础目标并按预测位置维持，且会抑制同一帧测量被多个目标复用，并在预测越出图像边界或最近 `numFramesLiving` 帧全无效时结束目标。最近 invalid 计数、连续 invalid 判断和有效率更新已复用 `lifecycle_utils`。`geoFullLeo=false` 时，`TrackTarget` 首片已按 legacy 非 FullLEO 分支使用 RA/Dec 匹配、RA/Dec 速度阈值和天区边界判断，配置项 `geoRa*Arcsec`/`geoDec*Arcsec` 已接入 JSON 读写。目标全部失活后会回到四帧关联入口，支持后续图像中重新发现目标。仍待迁移完整 legacy 分支，包括非 FullLEO `Assoc4`/`ReFindTargets` 的 RA/Dec 关联、外部校验 blob 生成、全局阈值策略和 TWDW/GDCL。

### 4. LeoTracker (`leo_tracker.h`)

LEO 目标跟踪器，对应 `TrackAlgo` 中 LEO 模式。

**当前状态:** `track()` 已完成 legacy `LEO_Assoc3`、`LEO_VerifyTarget` 和 `LEO_TrackTarget` 首批切片: 维护三帧 FIFO，按 AE 速度下限和两段 AE 运动一致性生成初始候选；第四帧可按预测 AE 位置匹配 blob，追加验证帧，用最近三段运动 median 更新预测，并选择 AE 速度最大的已验证目标；第四帧未命中时会追加 invalid 帧、丢弃未验证候选，并允许后续帧重新进入三帧关联完成重发现；验证后的第五帧有效测量会继续按预测 AE 匹配、追加并推进下一帧预测，后续单帧 miss 会按预测位置追加 invalid 帧并保持目标存活，连续 5 帧 miss 后按 legacy 规则释放目标。帧信息构造、AE 最近匹配、invalid fallback 和 median 预测更新已复用 `prediction_utils`，连续 invalid living policy 已复用 `lifecycle_utils`。TWDW/GDCL 仍待补齐。

### 5. ScTracker (`sc_tracker.h`)

星表跟踪器 (Space Catalog)，对应 `TrackAlgo` 中 SC 模式。

**当前状态:** `track()` 已完成 legacy `SC_Assoc3`、`SC_VerifyTarget` 和 `SC_TrackTarget` 首批切片: 维护三帧 FIFO，按像素位移半径、FOV 中心窗口和两段像素运动一致性生成初始候选，并按 oldsrc 原始候选全集比较压缩复用初始测量点的相似轨迹，链式重复场景已覆盖；第四帧可按预测像素位置匹配 blob，追加验证帧，用最近三段运动 median 更新预测，并选择最新 blob 面积最大的已验证目标；验证分支使用 validity-window living policy，验证后的下一帧会继续按预测像素位置和 FOV 中心窗口匹配，TrackTarget 分支使用 legacy 活动逻辑中的 latest-valid 释放策略，miss 后允许后续帧重新进入三帧关联完成重发现。候选去重已复用 `candidate_utils`；帧信息构造、Frame 最近匹配、FOV 中心门控、invalid fallback 和 median 预测更新已复用 `prediction_utils`；latest valid、validity window 与 living policy 入口已复用 `lifecycle_utils`。TWDW/GDCL 仍待补齐。

### 6. ManualTracker (`manual_tracker.h`)

手动跟踪器，用户点击选定目标后在后续帧中跟踪。

| 方法 | 说明 |
|------|------|
| `setManualTarget(blob)` | 设置手动选定的人工目标 blob |
| `track(measurements)` | 将人工 blob 转换为当前帧 `TargetInfo` 并维护预测状态 |

**当前状态:** Manual 最小闭环已完成。未选择目标时不输出结果；选择目标后会补齐人工 blob 的边界框、DN/area、AE 坐标、距离修正和相邻帧速度，并输出 `TargetInfo`。旧版 `MANUAL_Assoc3`、`MANUAL_VerifyTarget`、`MANUAL_TrackTarget` 的完整历史校验和 TWDW/GDCL 细节仍待后续迁移。

### 7. 数学工具 (`math_utils.h`) — 命名空间 `Dss::Math`

从旧版 `mpolyfit`、`mfft`、`getperiod` 移植的数学函数，**已完全移植且有测试覆盖**。
其中 `legacyFftSpectrum()` 对齐旧版 `mFFT::FFT_process` 的可观察输出，包括非 2 的幂输入补零、DC/普通频点幅值归一、相位角和基础频率。

| 函数 | 旧版来源 | 用途 |
|------|---------|------|
| `polyfit()` / `polynomialFit()` / `linearFit()` | `mpolyfit` | 最小二乘多项式拟合和统计量 |
| `polyval()` | `mpolyfit` | 多项式求值 |
| `legacyFftSize()` / `legacyFftSpectrum()` | `mfft` | 旧版频谱尺寸、幅值、相位、基频兼容 |
| `fft()` / `ifft()` | `mfft` | DFT/逆变换基础函数 |
| `samplePeriodError()` / `minimumSamplePeriod()` | `getperiod` | 采样周期误差和最小周期估计 |
| `removePolynomialTrend()` / `legacyNearestSampleInterpolate()` | `getperiod` | 去趋势和旧版最近采样插值 |
| `estimatePeriod(data, sampleRate)` | `getperiod` | 信号周期估计 |
| `median(data)` | — | 中位数计算 |

## 跟踪算法对照表

| 旧版 `TrackAlgo` 函数 | 新类 | 迁移状态 |
|----------------------|------|---------|
| `TrackProc_GEO()` | `GeoTracker::track()` | **函数级首版完成**，完整 legacy 分支待补 |
| `TrackProc_LEO()` | `LeoTracker::track()` | **Assoc3/VerifyTarget/TrackTarget 首片完成**，三帧初始关联、第四帧验证、第五帧持续跟踪、单帧跟踪 miss、连续 5 帧 miss 释放目标和验证失败后重发现已覆盖 |
| `TrackProc_SC()` | `ScTracker::track()` | **Assoc3/VerifyTarget/TrackTarget 首片完成**，三帧初始关联、按 oldsrc 原始候选全集比较的重复候选压缩、第四帧验证、verify/track living policy、第五帧持续跟踪、miss 后重发现已覆盖 |
| `TrackProc_MANUAL()` | `ManualTracker::track()` | **最小闭环完成**，legacy 三帧关联/校验细节待迁移 |
| `calcStarSpeed()` | `GeoTracker` / `estimateGeoStarSpeed()` | **首版完成**，中心视场星速和 AE 换算已覆盖 |
| `assoc4()` | `GeoTracker` 内方法 | **首版完成**，四帧关联、星速过滤、运动一致性过滤已覆盖 |
| `findTargets()` | `GeoTracker` 内方法 | **基础完成**，按关联结果设置发现/验证状态 |
| `trackTargets()` | `GeoTracker` 内方法 | **基础完成**，按预测位置维持目标，已补同帧测量复用抑制、预测越界/连续无效结束和丢失后重发现状态切换，完整 legacy 校验待补 |
| `mpolyfit` / `mfft` / `getperiod` | `Dss::Math::*` | **已完成**，包含 legacy FFT 频谱兼容 helper |

## 当前缺口

| 缺口 | 严重程度 | 说明 |
|------|---------|------|
| GEO 完整 legacy 分支待补 | **高** | 星速、四帧关联、基础维持、测量复用抑制、living 规则、非 FullLEO RA/Dec TrackTarget 首片和丢失后重发现入口已迁移；完整非 FullLEO `Assoc4`/`GEO_ReFindTargets` RA/Dec 关联、外部校验 blob 生成、全局阈值、TWDW/GDCL 仍需继续对照旧版补齐 |
| LEO/SC 算法未完整移植 | **高** | LEO 三帧初始关联、第四帧验证、第五帧持续跟踪、单帧跟踪 miss、连续 5 帧 miss 释放目标和验证失败后重发现首片已迁移；SC 三帧初始关联、重复候选压缩、第四帧验证、verify/track living policy、第五帧持续跟踪和 miss 后重发现首片已迁移；两者已复用 `candidate_utils`、`prediction_utils` 和 `lifecycle_utils` 的公共 helper；TWDW/GDCL 仍待补齐 |
| Manual legacy 细节待补 | 中 | 最小选点闭环已完成，三帧关联、验证、TWDW/GDCL 仍需对照旧版迁移 |
| 指向误差模型 | 中 | `PointingErrorResult` 已定义但计算逻辑未移植 |

## 依赖关系

```
dss_tracking
└── dss_core
```
