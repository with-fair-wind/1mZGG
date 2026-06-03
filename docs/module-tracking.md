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
| `setMode(mode, settings)` | 按模式切换策略 (含 TODO: 工厂方法) |
| `track(measurements)` | 委托当前策略执行跟踪 |
| `currentMode()` | 获取当前跟踪模式 |
| `reset()` | 重置策略状态 |

### 3. GeoTracker (`geo_tracker.h`)

GEO 目标跟踪器，对应旧版 `TrackAlgo` 中 GEO 模式的算法。

**旧版核心算法 (待移植):**
- `calcStarSpeed()` — 星体运动速度计算
- `assoc4()` — 四帧关联
- `findTargets()` — 新目标发现
- `trackTargets()` — 已知目标跟踪维持

**当前状态:** 类骨架已建立，所有核心方法体为 TODO。

### 4. LeoTracker (`leo_tracker.h`)

LEO 目标跟踪器，对应 `TrackAlgo` 中 LEO 模式。

**当前状态:** `track()` 方法体为 TODO。

### 5. ScTracker (`sc_tracker.h`)

星表跟踪器 (Space Catalog)，对应 `TrackAlgo` 中 SC 模式。

**当前状态:** `track()` 方法体为 TODO。

### 6. ManualTracker (`manual_tracker.h`)

手动跟踪器，用户点击选定目标后在后续帧中跟踪。

| 方法 | 说明 |
|------|------|
| `setManualTarget(x, y)` | 设置手动选定的目标位置 |
| `track(measurements)` | 在目标列表中搜索最近匹配 |

**当前状态:** `track()` 方法体为 TODO。

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
| `TrackProc_GEO()` | `GeoTracker::track()` | 骨架，算法未移植 |
| `TrackProc_LEO()` | `LeoTracker::track()` | 骨架，算法未移植 |
| `TrackProc_SC()` | `ScTracker::track()` | 骨架，算法未移植 |
| `TrackProc_MANUAL()` | `ManualTracker::track()` | 骨架，算法未移植 |
| `calcStarSpeed()` | `GeoTracker` 内方法 | 骨架 |
| `assoc4()` | `GeoTracker` 内方法 | 骨架 |
| `findTargets()` | `GeoTracker` 内方法 | 骨架 |
| `trackTargets()` | `GeoTracker` 内方法 | 骨架 |
| `mpolyfit` / `mfft` / `getperiod` | `Dss::Math::*` | **已完成**，包含 legacy FFT 频谱兼容 helper |

## 当前缺口

| 缺口 | 严重程度 | 说明 |
|------|---------|------|
| 四种跟踪算法未移植 | **高** | 旧版 `TrackAlgo.cpp` (~3000行) 是最大的未迁移代码块 |
| `TrackManager::setMode()` 工厂 | 中 | 需实现按模式自动创建对应 Tracker 实例 |
| 指向误差模型 | 中 | `PointingErrorResult` 已定义但计算逻辑未移植 |

## 依赖关系

```
dss_tracking
└── dss_core
```
