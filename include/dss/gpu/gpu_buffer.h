#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

#include <cuda_runtime.h>

namespace Dss::Gpu {

/**
 * @brief GPU 设备端缓冲区，封装 cudaMalloc/cudaFree 与主机-设备数据传输
 * @tparam T 元素类型
 */
template <typename T>
class GpuBuffer {
public:
    GpuBuffer() = default;

    /**
     * @brief 分配指定元素数量的 GPU 内存
     * @param count 元素个数；为 0 时不分配
     * @throws std::runtime_error cudaMalloc 失败时抛出
     */
    explicit GpuBuffer(size_t count) : m_count(count) {
        if (m_count > 0) {
            auto err = cudaMalloc(&m_ptr, m_count * sizeof(T));
            if (err != cudaSuccess) {
                throw std::runtime_error(std::string("cudaMalloc failed: ") +
                                         cudaGetErrorString(err));
            }
        }
    }

    ~GpuBuffer() noexcept {
        free();
    }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    /// 移动构造，接管 other 的设备指针与元素计数
    GpuBuffer(GpuBuffer&& other) noexcept
        : m_ptr(std::exchange(other.m_ptr, nullptr)), m_count(std::exchange(other.m_count, 0)) {}

    /// 移动赋值，释放当前资源后接管 other 的设备指针与元素计数
    GpuBuffer& operator=(GpuBuffer&& other) noexcept {
        if (this != &other) {
            free();
            m_ptr = std::exchange(other.m_ptr, nullptr);
            m_count = std::exchange(other.m_count, 0);
        }
        return *this;
    }

    /**
     * @brief 将主机数据上传到 GPU
     * @param hostData 主机端数据源
     * @param stream 可选的 CUDA 流；为 nullptr 时使用同步拷贝
     */
    void upload(std::span<const T> hostData, cudaStream_t stream = nullptr) {
        auto bytes = hostData.size() * sizeof(T);
        if (stream) {
            cudaMemcpyAsync(m_ptr, hostData.data(), bytes, cudaMemcpyHostToDevice, stream);
        } else {
            cudaMemcpy(m_ptr, hostData.data(), bytes, cudaMemcpyHostToDevice);
        }
    }

    /**
     * @brief 从 GPU 下载数据到主机
     * @param hostData 主机端目标缓冲区
     * @param stream 可选的 CUDA 流；为 nullptr 时使用同步拷贝
     */
    void download(std::span<T> hostData, cudaStream_t stream = nullptr) const {
        auto bytes = hostData.size() * sizeof(T);
        if (stream) {
            cudaMemcpyAsync(hostData.data(), m_ptr, bytes, cudaMemcpyDeviceToHost, stream);
        } else {
            cudaMemcpy(hostData.data(), m_ptr, bytes, cudaMemcpyDeviceToHost);
        }
    }

    /**
     * @brief 将缓冲区内容清零
     * @param stream 可选的 CUDA 流；为 nullptr 时使用同步 memset
     */
    void zero(cudaStream_t stream = nullptr) {
        if (m_ptr && m_count > 0) {
            if (stream) {
                cudaMemsetAsync(m_ptr, 0, m_count * sizeof(T), stream);
            } else {
                cudaMemset(m_ptr, 0, m_count * sizeof(T));
            }
        }
    }

    /// 获取可写的设备端指针
    [[nodiscard]] auto devicePtr() -> T* {
        return m_ptr;
    }

    /// 获取只读的设备端指针
    [[nodiscard]] auto devicePtr() const -> const T* {
        return m_ptr;
    }

    /// 获取元素个数
    [[nodiscard]] auto size() const -> size_t {
        return m_count;
    }

    /// 获取占用字节数
    [[nodiscard]] auto bytes() const -> size_t {
        return m_count * sizeof(T);
    }

    /// 缓冲区是否已分配有效设备内存
    [[nodiscard]] bool valid() const {
        return m_ptr != nullptr;
    }

private:
    /// 释放设备内存并重置状态
    void free() noexcept {
        if (m_ptr) {
            cudaFree(m_ptr);
            m_ptr = nullptr;
            m_count = 0;
        }
    }

    T* m_ptr = nullptr;  ///< 设备端数据指针
    size_t m_count = 0;  ///< 元素个数
};

}  // namespace Dss::Gpu
