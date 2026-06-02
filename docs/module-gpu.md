# GPU 模块 (`dss_gpu_cuda`)

> 命名空间: `Dss::Gpu`
>
> 头文件: `include/dss/gpu/`
>
> 源文件: `src/gpu/`、`kernels/`
>
> 依赖: `dss_core`, `CUDA::cudart`
>
> 构建选项: `DSS_ENABLE_CUDA=ON` (默认 OFF)

## 模块职责

GPU 模块将旧版 OpenCL 计算路径迁移为 CUDA 实现，提供高性能的图像处理核函数。当前核函数已移植但尚未接入处理管线。

## 组件清单

### 1. IGpuBackend (`i_gpu_backend.h`，隐含)

GPU 后端初始化接口。

### 2. CudaDeviceManager (`cuda_device_manager.h`)

CUDA 设备管理器，从旧版 `DeviceManager` (OpenCL) 迁移。

| 成员 | 说明 |
|------|------|
| 设备探测 | 自动选择最佳 CUDA 设备 |
| 4 个 CUDA 流 | 支持并行核函数执行 |

### 3. GpuBuffer\<T\> (`gpu_buffer.h`，隐含)

RAII 设备内存管理：

- 构造时 `cudaMalloc`
- 析构时 `cudaFree`
- 支持 Host ↔ Device 数据传输

### 4. CUDA 核函数 (`kernels/`)

| 文件 | 旧版 OpenCL 对应 | 核函数内容 |
|------|-----------------|-----------|
| `statistics.cu` | `KernelCL.cl` 统计部分 | 图像统计 (max/min/avg/σ) |
| `filter.cu` | `KernelCL.cl` 滤波部分 | 中值滤波、均值滤波 |
| `transform.cu` | `KernelCL.cl` 变换部分 | 图像旋转、翻转 |
| `arithmetic.cu` | `KernelCL.cl` 算术部分 | 帧差、帧加减 |
| `calibration.cu` | `KernelCL.cl` 定标部分 | 暗场/平场校正 |
| `composite.cu` | `KernelCL.cl` 合成部分 | 多帧合成 |

### 5. 核函数声明 (`cuda_kernels.h`)

C++ 主机端可调用的核函数包装声明。

## OpenCL → CUDA 对照

| OpenCL (`KernelCL.cl`) | CUDA (`kernels/*.cu`) | 状态 |
|------------------------|----------------------|------|
| 统计核函数 | `statistics.cu` | 已移植 |
| 滤波核函数 | `filter.cu` | 已移植 |
| 几何变换 | `transform.cu` | 已移植 |
| 算术运算 | `arithmetic.cu` | 已移植 |
| 定标校正 | `calibration.cu` | 已移植 |
| 多帧合成 | `composite.cu` | 已移植 |

## CMake 配置

```cmake
option(DSS_ENABLE_CUDA "Build CUDA GPU backend and kernels" OFF)

# CUDA 标准: C++20
# 目标架构: 50, 60, 70, 75, 80, 86, 89, 90
# 分离编译: ON
```

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 未接入处理管线 | 核函数存在但无 `IProcessingStrategy` 的 CUDA 实现 |
| `CudaDeviceManager` 未初始化 | `main.cpp` 中有 TODO 但未执行 |
| 默认关闭 | `DSS_ENABLE_CUDA=OFF`，需手动开启 |
| 无 GPU 单元测试 | CUDA 核函数缺少测试 |

## 依赖关系

```
dss_gpu_cuda
├── dss_core
└── CUDA::cudart
```
