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

    /// @brief 释放持有的设备内存。
    ~GpuBuffer() noexcept {
        free();
    }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    /**
     * @brief 移动构造并接管设备内存。
     * @param other 被置为空状态的源缓冲区。
     */
    GpuBuffer(GpuBuffer&& other) noexcept
        : m_ptr(std::exchange(other.m_ptr, nullptr)), m_count(std::exchange(other.m_count, 0)) {}

    /**
     * @brief 释放当前资源后接管另一个缓冲区。
     * @param other 被置为空状态的源缓冲区。
     * @return 当前缓冲区。
     */
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

    /** @brief 获取可写设备指针。 @return 设备内存地址；未分配时返回 nullptr。 */
    [[nodiscard]] auto devicePtr() -> T* {
        return m_ptr;
    }

    /** @brief 获取只读设备指针。 @return 设备内存地址；未分配时返回 nullptr。 */
    [[nodiscard]] auto devicePtr() const -> const T* {
        return m_ptr;
    }

    /** @brief 获取元素数量。 @return 当前分配可容纳的 T 元素数。 */
    [[nodiscard]] auto size() const -> size_t {
        return m_count;
    }

    /** @brief 获取占用字节数。 @return 元素数量与 sizeof(T) 的乘积。 */
    [[nodiscard]] auto bytes() const -> size_t {
        return m_count * sizeof(T);
    }

    /** @brief 查询是否持有设备内存。 @return 指针非空时返回 true。 */
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
