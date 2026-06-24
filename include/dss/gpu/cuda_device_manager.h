#pragma once

#include <array>
#include <expected>
#include <string>

#include <cuda_runtime.h>

#include "dss/gpu/i_gpu_backend.h"

namespace Dss::Gpu {

/// CUDA 设备管理器，负责设备选择、流创建与同步。
class CudaDeviceManager final : public IGpuBackend {
public:
    static constexpr int NumStreams = 4;  ///< 预创建的 CUDA 流数量

    CudaDeviceManager() = default;
    ~CudaDeviceManager() override;

    CudaDeviceManager(const CudaDeviceManager&) = delete;
    CudaDeviceManager& operator=(const CudaDeviceManager&) = delete;

    /**
     * @brief 初始化 CUDA 设备并创建流
     * @return 成功时返回空值，失败时返回错误描述
     */
    auto init() -> std::expected<void, std::string> override;

    /** @brief 查询 CUDA 管理器是否已初始化。 @return 设备和流均可用时返回 true。 */
    [[nodiscard]] bool isInitialized() const override {
        return m_initialized;
    }

    /** @brief 获取 CUDA 设备名称。 @return 初始化时读取的设备名称。 */
    [[nodiscard]] auto deviceName() const -> std::string override {
        return m_deviceName;
    }

    /**
     * @brief 获取指定索引的 CUDA 流
     * @param index 流索引，默认为 0
     * @return 有效的 cudaStream_t；索引越界时返回 nullptr
     */
    [[nodiscard]] auto stream(int index = 0) const -> cudaStream_t;

    /// 同步所有已创建的 CUDA 流
    void synchronizeAll() const;

    /**
     * @brief 同步指定索引的 CUDA 流
     * @param streamIndex 流索引，默认为 0
     */
    void synchronize(int streamIndex = 0) const;

    /**
     * @brief 获取设备计算能力。
     * @return 由主版本号和次版本号组成的数对。
     */
    [[nodiscard]] auto computeCapability() const -> std::pair<int, int> {
        return {m_ccMajor, m_ccMinor};
    }

    /** @brief 获取设备全局内存总量。 @return 驱动报告的字节数。 */
    [[nodiscard]] auto totalMemory() const -> size_t {
        return m_totalMemory;
    }

private:
    /// @brief 销毁所有已创建的 CUDA 流并重置句柄。
    void releaseStreams() noexcept;

    bool m_initialized = false;                        ///< 是否已完成初始化
    int m_deviceId = 0;                                ///< 当前使用的 CUDA 设备 ID
    int m_ccMajor = 0;                                 ///< 计算能力主版本号
    int m_ccMinor = 0;                                 ///< 计算能力次版本号
    size_t m_totalMemory = 0;                          ///< 设备全局内存总量（字节）
    std::string m_deviceName;                          ///< 设备名称
    std::array<cudaStream_t, NumStreams> m_streams{};  ///< 预创建的 CUDA 流数组
};

}  // namespace Dss::Gpu
