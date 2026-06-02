#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

#include <cuda_runtime.h>

namespace Dss::Gpu {

template <typename T>
class GpuBuffer {
public:
    GpuBuffer() = default;

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

    GpuBuffer(GpuBuffer&& other) noexcept
        : m_ptr(std::exchange(other.m_ptr, nullptr)), m_count(std::exchange(other.m_count, 0)) {}

    GpuBuffer& operator=(GpuBuffer&& other) noexcept {
        if (this != &other) {
            free();
            m_ptr = std::exchange(other.m_ptr, nullptr);
            m_count = std::exchange(other.m_count, 0);
        }
        return *this;
    }

    void upload(std::span<const T> hostData, cudaStream_t stream = nullptr) {
        auto bytes = hostData.size() * sizeof(T);
        if (stream) {
            cudaMemcpyAsync(m_ptr, hostData.data(), bytes, cudaMemcpyHostToDevice, stream);
        } else {
            cudaMemcpy(m_ptr, hostData.data(), bytes, cudaMemcpyHostToDevice);
        }
    }

    void download(std::span<T> hostData, cudaStream_t stream = nullptr) const {
        auto bytes = hostData.size() * sizeof(T);
        if (stream) {
            cudaMemcpyAsync(hostData.data(), m_ptr, bytes, cudaMemcpyDeviceToHost, stream);
        } else {
            cudaMemcpy(hostData.data(), m_ptr, bytes, cudaMemcpyDeviceToHost);
        }
    }

    void zero(cudaStream_t stream = nullptr) {
        if (m_ptr && m_count > 0) {
            if (stream) {
                cudaMemsetAsync(m_ptr, 0, m_count * sizeof(T), stream);
            } else {
                cudaMemset(m_ptr, 0, m_count * sizeof(T));
            }
        }
    }

    [[nodiscard]] auto devicePtr() -> T* {
        return m_ptr;
    }
    [[nodiscard]] auto devicePtr() const -> const T* {
        return m_ptr;
    }
    [[nodiscard]] auto size() const -> size_t {
        return m_count;
    }
    [[nodiscard]] auto bytes() const -> size_t {
        return m_count * sizeof(T);
    }
    [[nodiscard]] bool valid() const {
        return m_ptr != nullptr;
    }

private:
    void free() noexcept {
        if (m_ptr) {
            cudaFree(m_ptr);
            m_ptr = nullptr;
            m_count = 0;
        }
    }

    T* m_ptr = nullptr;
    size_t m_count = 0;
};

}  // namespace Dss::Gpu
