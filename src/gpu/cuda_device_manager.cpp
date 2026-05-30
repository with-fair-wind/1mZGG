#include "dss/gpu/cuda_device_manager.h"

#include <format>

namespace Dss::Gpu
{

CudaDeviceManager::~CudaDeviceManager()
{
    if (m_initialized)
    {
        for (auto& s : m_streams)
        {
            if (s)
            {
                cudaStreamDestroy(s);
                s = nullptr;
            }
        }
    }
}

auto CudaDeviceManager::init() -> std::expected<void, std::string>
{
    int deviceCount = 0;
    auto err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0)
    {
        return std::unexpected("No CUDA-capable devices found");
    }

    cudaDeviceProp prop{};
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess)
    {
        return std::unexpected(std::format("cudaGetDeviceProperties failed: {}", cudaGetErrorString(err)));
    }

    m_deviceId = 0;
    m_deviceName = prop.name;
    m_ccMajor = prop.major;
    m_ccMinor = prop.minor;
    m_totalMemory = prop.totalGlobalMem;

    err = cudaSetDevice(m_deviceId);
    if (err != cudaSuccess)
    {
        return std::unexpected(std::format("cudaSetDevice failed: {}", cudaGetErrorString(err)));
    }

    for (auto& s : m_streams)
    {
        err = cudaStreamCreate(&s);
        if (err != cudaSuccess)
        {
            return std::unexpected(std::format("cudaStreamCreate failed: {}", cudaGetErrorString(err)));
        }
    }

    m_initialized = true;
    return {};
}

auto CudaDeviceManager::stream(int index) const -> cudaStream_t
{
    if (index < 0 || index >= NumStreams)
    {
        return nullptr;
    }
    return m_streams[static_cast<size_t>(index)];
}

void CudaDeviceManager::synchronizeAll() const
{
    for (const auto& s : m_streams)
    {
        if (s)
        {
            cudaStreamSynchronize(s);
        }
    }
}

void CudaDeviceManager::synchronize(int streamIndex) const
{
    if (auto s = stream(streamIndex))
    {
        cudaStreamSynchronize(s);
    }
}

} // namespace Dss::Gpu
