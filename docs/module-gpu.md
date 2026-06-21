# GPU 模块 (`dss_gpu_cuda`)

> 命名空间: 设备与缓冲区使用 `Dss::Gpu`；处理策略使用 `Dss::Processing`
>
> 头文件: `include/dss/gpu/`
>
> 源文件: `src/gpu/`、`src/processing/cuda_processing_strategy.cpp`、`kernels/`
>
> 依赖: `dss_processing`, `CUDA::cudart`
>
> 构建选项: `DSS_ENABLE_CUDA=ON` (默认 OFF)

## 模块职责

GPU 模块将旧版 OpenCL 计算路径迁移为 CUDA 实现。核函数、设备与缓冲区生命周期已由可选 `CudaProcessingStrategy` 接入处理管线；默认构建仍关闭 CUDA。

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

### 5. CudaProcessingStrategy (`cuda_processing_strategy.h`)

可选 `IProcessingStrategy` 实现，使用 `CudaDeviceManager` 和 `GpuBuffer` 执行统计、阈值与公共 `Labeler` 目标提取；创建失败通过 `std::expected` 返回，不影响无 CUDA 应用启动。

### 6. 核函数声明 (`cuda_kernels.h`)

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
| 硬件正确性 | 固定帧 CPU/OpenCV/CUDA 对照测试已定义，需在 CUDA 设备执行并回填结果 |
| 性能收益 | 6144 级基准入口已提供，需记录延迟、吞吐、显存和丢帧指标 |
| UI 启用 | `DSS_ENABLE_CUDA=OFF` 保持默认安全；仅在基准达到门槛后增加 UI 模式 |

执行命令、收益门槛和结果表见 [硬件验证](hardware-validation.md)。

## 依赖关系

```
dss_gpu_cuda
├── dss_processing
└── CUDA::cudart
```
