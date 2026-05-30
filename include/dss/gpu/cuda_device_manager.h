#pragma once

#include "dss/gpu/i_gpu_backend.h"

#include <cuda_runtime.h>

#include <array>
#include <expected>
#include <string>

namespace Dss::Gpu
{

class CudaDeviceManager final : public IGpuBackend
{
public:
    static constexpr int NumStreams = 4;

    CudaDeviceManager() = default;
    ~CudaDeviceManager() override;

    CudaDeviceManager(const CudaDeviceManager&) = delete;
    CudaDeviceManager& operator=(const CudaDeviceManager&) = delete;

    auto init() -> std::expected<void, std::string> override;
    [[nodiscard]] bool isInitialized() const override { return m_initialized; }
    [[nodiscard]] auto deviceName() const -> std::string override { return m_deviceName; }

    [[nodiscard]] auto stream(int index = 0) const -> cudaStream_t;
    void synchronizeAll() const;
    void synchronize(int streamIndex = 0) const;

    [[nodiscard]] auto computeCapability() const -> std::pair<int, int> { return {m_ccMajor, m_ccMinor}; }
    [[nodiscard]] auto totalMemory() const -> size_t { return m_totalMemory; }

private:
    bool m_initialized = false;
    int m_deviceId = 0;
    int m_ccMajor = 0;
    int m_ccMinor = 0;
    size_t m_totalMemory = 0;
    std::string m_deviceName;
    std::array<cudaStream_t, NumStreams> m_streams{};
};

} // namespace Dss::Gpu
