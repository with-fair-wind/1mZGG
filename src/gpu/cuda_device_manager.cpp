#include "dss/gpu/cuda_device_manager.h"

#include <format>

namespace Dss::Gpu {

/// 析构时销毁所有已创建的 CUDA 流
CudaDeviceManager::~CudaDeviceManager() {
    releaseStreams();
}

void CudaDeviceManager::releaseStreams() noexcept {
    for (auto& stream : m_streams) {
        if (stream != nullptr) {
            (void)cudaStreamDestroy(stream);
            stream = nullptr;
        }
    }
    m_initialized = false;
}

/**
 * @brief 选择首个可用 CUDA 设备，查询属性并创建流
 * @return 成功时返回空值；无设备或 CUDA API 失败时返回错误描述
 */
auto CudaDeviceManager::init() -> std::expected<void, std::string> {
    releaseStreams();

    int deviceCount = 0;
    auto err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0) {
        return std::unexpected("No CUDA-capable devices found");
    }

    cudaDeviceProp prop{};
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        return std::unexpected(
            std::format("cudaGetDeviceProperties failed: {}", cudaGetErrorString(err)));
    }

    m_deviceId = 0;
    m_deviceName = prop.name;
    m_ccMajor = prop.major;
    m_ccMinor = prop.minor;
    m_totalMemory = prop.totalGlobalMem;

    err = cudaSetDevice(m_deviceId);
    if (err != cudaSuccess) {
        return std::unexpected(std::format("cudaSetDevice failed: {}", cudaGetErrorString(err)));
    }

    for (auto& s : m_streams) {
        err = cudaStreamCreate(&s);
        if (err != cudaSuccess) {
            const auto message =
                std::format("cudaStreamCreate failed: {}", cudaGetErrorString(err));
            releaseStreams();
            return std::unexpected(message);
        }
    }

    m_initialized = true;
    return {};
}

// 按索引返回预创建的 CUDA 流；公共契约记录在头文件中。
auto CudaDeviceManager::stream(int index) const -> cudaStream_t {
    if (index < 0 || index >= NumStreams) {
        return nullptr;
    }
    return m_streams[static_cast<size_t>(index)];
}

/// 依次同步全部 NumStreams 条 CUDA 流
void CudaDeviceManager::synchronizeAll() const {
    for (const auto& s : m_streams) {
        if (s) {
            cudaStreamSynchronize(s);
        }
    }
}

// 同步指定索引的 CUDA 流；公共契约记录在头文件中。
void CudaDeviceManager::synchronize(int streamIndex) const {
    if (auto s = stream(streamIndex)) {
        cudaStreamSynchronize(s);
    }
}

}  // namespace Dss::Gpu
