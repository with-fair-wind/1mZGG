# 硬件验证手册

本文档用于完成默认开发机无法执行的 Sapera 与 CUDA 验收。两类能力均保持显式启用，默认构建不会打开设备。

## Sapera smoke test

配置时必须提供 Sapera SDK 根目录；SDK 缺失会在 CMake 阶段直接失败。

```powershell
cmake -S . -B build/msvc-sapera -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DDSS_ENABLE_SAPERA=ON `
  -DSAPERA_ROOT=C:/Teledyne/Sapera `
  -DCMAKE_TOOLCHAIN_FILE=build/msvc-release/generators/conan_toolchain.cmake
cmake --build build/msvc-sapera --target dss_sapera_smoke
./build/msvc-sapera/bin/dss_sapera_smoke.exe C:/camera/camera.ccf 100 30
```

通过条件：初始化和启动均成功；30 秒内收到 100 帧；每帧像素数等于宽乘高；停止后进程正常退出。失败退出码和错误文本可直接用于采集机日志。

## CUDA 正确性与基准

```powershell
cmake -S . -B build/msvc-cuda -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DDSS_ENABLE_CUDA=ON `
  -DDSS_ENABLE_TESTS=ON `
  -DCMAKE_TOOLCHAIN_FILE=build/msvc-release/generators/conan_toolchain.cmake
cmake --build build/msvc-cuda --target test_cuda_processing_contract dss_processing_benchmark
ctest --test-dir build/msvc-cuda -R CudaProcessingContract --output-on-failure
./build/msvc-cuda/bin/dss_processing_benchmark.exe
```

基准输出包含 6144 x 6144 固定帧的 OpenCV/CUDA 平均延迟、吞吐、丢帧数和 CUDA 设备缓冲字节数。

CUDA 进入 UI 的门槛：固定帧 blob 数量、面积和质心对照测试通过；连续三次 Release 基准无丢帧；CUDA 吞吐至少为 OpenCV 的 1.5 倍。未达到门槛时保留可选后端和基准，不增加 UI 选项。

## 结果记录

| 日期 | 机器/设备 | SDK/驱动 | 测试 | 结果 | 证据路径 |
|---|---|---|---|---|---|
| 待填写 | 待填写 | 待填写 | Sapera smoke | 待执行 | 待填写 |
| 待填写 | 待填写 | 待填写 | CUDA contract/benchmark | 待执行 | 待填写 |