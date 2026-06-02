#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__global__ void kernel_hist16(const uint16_t* image, uint32_t* histogram, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid < n) {
        atomicAdd(&histogram[image[gid]], 1u);
    }
}

__global__ void kernel_avg_group_sum16(const uint16_t* src, double* out, int n, int groups) {
    extern __shared__ double shared_sum[];

    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;
    int count = n / stride;

    double partial = 0.0;
    int idx = gid;
    for (int i = 0; i < count; ++i, idx += stride) {
        if (idx < n) {
            partial += static_cast<double>(src[idx]);
        }
    }

    shared_sum[threadIdx.x] = partial;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        out[blockIdx.x] = shared_sum[0];
    }
}

__global__ void kernel_avg_group_sum8(const uint8_t* src, double* out, int n, int groups) {
    extern __shared__ double shared_sum[];

    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;
    int count = n / stride;

    double partial = 0.0;
    int idx = gid;
    for (int i = 0; i < count; ++i, idx += stride) {
        if (idx < n) {
            partial += static_cast<double>(src[idx]);
        }
    }

    shared_sum[threadIdx.x] = partial;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        out[blockIdx.x] = shared_sum[0];
    }
}

__global__ void kernel_var_group_sum16(const uint16_t* src, double avg, double* out, int n) {
    extern __shared__ double shared_sum[];

    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;

    double partial = 0.0;
    for (int idx = gid; idx < n; idx += stride) {
        double diff = static_cast<double>(src[idx]) - avg;
        partial += diff * diff;
    }

    shared_sum[threadIdx.x] = partial;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        out[blockIdx.x] = shared_sum[0];
    }
}

__global__ void kernel_global_sum(double* data, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += data[i];
    }
    data[0] = sum;
}

__global__ void kernel_min_max_group16(const uint16_t* src, double* gmin, double* gmax, int n) {
    extern __shared__ double shared_data[];  // first half: min, second half: max
    double* smin = shared_data;
    double* smax = shared_data + blockDim.x;

    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;

    double local_min = 1e30;
    double local_max = -1e30;
    for (int idx = gid; idx < n; idx += stride) {
        double val = static_cast<double>(src[idx]);
        local_min = fmin(local_min, val);
        local_max = fmax(local_max, val);
    }

    smin[threadIdx.x] = local_min;
    smax[threadIdx.x] = local_max;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            smin[threadIdx.x] = fmin(smin[threadIdx.x], smin[threadIdx.x + s]);
            smax[threadIdx.x] = fmax(smax[threadIdx.x], smax[threadIdx.x + s]);
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        gmin[blockIdx.x] = smin[0];
        gmax[blockIdx.x] = smax[0];
    }
}

__global__ void kernel_global_min_max(double* gmin, double* gmax, int n) {
    double vmin = gmin[0];
    double vmax = gmax[0];
    for (int i = 1; i < n; ++i) {
        vmin = fmin(vmin, gmin[i]);
        vmax = fmax(vmax, gmax[i]);
    }
    gmin[0] = vmin;
    gmax[0] = vmax;
}

// --- Host wrapper functions ---

void hist16(const uint16_t* image, uint32_t* histogram, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_hist16<<<grid, block, 0, stream>>>(image, histogram, n);
}

void avg_group_sum16(const uint16_t* src, double* out, int n, int groups, cudaStream_t stream) {
    int block = 256;
    kernel_avg_group_sum16<<<groups, block, block * sizeof(double), stream>>>(src, out, n, groups);
}

void avg_group_sum8(const uint8_t* src, double* out, int n, int groups, cudaStream_t stream) {
    int block = 256;
    kernel_avg_group_sum8<<<groups, block, block * sizeof(double), stream>>>(src, out, n, groups);
}

void var_group_sum16(const uint16_t* src, double avg, double* out, int n, int groups,
                     cudaStream_t stream) {
    int block = 256;
    kernel_var_group_sum16<<<groups, block, block * sizeof(double), stream>>>(src, avg, out, n);
}

void var_group_sum8(const uint8_t* src, double avg, double* out, int n, int groups,
                    cudaStream_t stream) {
    (void)src;
    (void)avg;
    (void)out;
    (void)n;
    (void)groups;
    (void)stream;
    // TODO: implement 8-bit variant
}

void global_sum(double* data, int n, cudaStream_t stream) {
    kernel_global_sum<<<1, 1, 0, stream>>>(data, n);
}

void min_max_group16(const uint16_t* src, double* gmin, double* gmax, int n, int groups,
                     cudaStream_t stream) {
    int block = 256;
    kernel_min_max_group16<<<groups, block, 2 * block * sizeof(double), stream>>>(src, gmin, gmax,
                                                                                  n);
}

void min_max_group8(const uint8_t* src, double* gmin, double* gmax, int n, int groups,
                    cudaStream_t stream) {
    (void)src;
    (void)gmin;
    (void)gmax;
    (void)n;
    (void)groups;
    (void)stream;
    // TODO: implement 8-bit variant
}

void global_min_max(double* gmin, double* gmax, int n, cudaStream_t stream) {
    kernel_global_min_max<<<1, 1, 0, stream>>>(gmin, gmax, n);
}

}  // namespace Dss::Gpu
