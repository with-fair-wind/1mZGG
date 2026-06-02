#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__global__ void kernel_short2byte(const uint16_t* src, uint8_t* dst, int min_val, int max_val,
                                  float diff, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }

    if (src[gid] <= static_cast<uint16_t>(min_val)) {
        dst[gid] = 0;
    } else if (src[gid] >= static_cast<uint16_t>(max_val)) {
        dst[gid] = 255;
    } else {
        float rate = static_cast<float>(src[gid] - min_val) / diff;
        dst[gid] = static_cast<uint8_t>(rate * 255.0f);
    }
}

__global__ void kernel_sub_frame16(const uint16_t* src1, const uint16_t* src2, uint16_t* dst, int w,
                                   int h, int shift_x, int shift_y) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    int x2 = x + shift_x;
    int y2 = y + shift_y;
    int dst_idx = y * w + x;

    if (x2 >= 0 && x2 < w && y2 >= 0 && y2 < h) {
        int val = static_cast<int>(src1[dst_idx]) - static_cast<int>(src2[y2 * w + x2]);
        dst[dst_idx] = static_cast<uint16_t>(max(0, val));
    } else {
        dst[dst_idx] = 0;
    }
}

__global__ void kernel_binary16(const uint16_t* src, uint8_t* dst, uint16_t threshold, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }
    dst[gid] = (src[gid] > threshold) ? 255 : 0;
}

__global__ void kernel_reduce_large_dn16(const uint16_t* src, uint16_t* dst, float thresh, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }
    dst[gid] = (static_cast<float>(src[gid]) > thresh) ? 0 : src[gid];
}

__global__ void kernel_reduce_small_dn16(const uint16_t* src, uint16_t* dst, float avg,
                                         float std_dev, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }
    float val = static_cast<float>(src[gid]);
    dst[gid] = (val < avg - 3.0f * std_dev) ? static_cast<uint16_t>(avg) : src[gid];
}

__global__ void kernel_reduce_saturation16(const uint16_t* src, uint16_t* dst, float avg,
                                           float std_dev, float ratio, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }
    float val = static_cast<float>(src[gid]);
    dst[gid] = (val > avg + ratio * std_dev) ? static_cast<uint16_t>(avg) : src[gid];
}

// --- Host wrappers ---

static dim3 grid2d(int w, int h, int bx = 16, int by = 16) {
    return dim3((w + bx - 1) / bx, (h + by - 1) / by);
}

void short_to_byte(const uint16_t* src, uint8_t* dst, int min_val, int max_val, float diff, int n,
                   cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_short2byte<<<grid, block, 0, stream>>>(src, dst, min_val, max_val, diff, n);
}

void sub_frame16(const uint16_t* src1, const uint16_t* src2, uint16_t* dst, int w, int h,
                 int shift_x, int shift_y, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_sub_frame16<<<grid2d(w, h), block, 0, stream>>>(src1, src2, dst, w, h, shift_x, shift_y);
}

void sub_frame8(const uint8_t* src1, const uint8_t* src2, uint8_t* dst, int w, int h, int shift_x,
                int shift_y, cudaStream_t stream) {
    (void)src1;
    (void)src2;
    (void)dst;
    (void)w;
    (void)h;
    (void)shift_x;
    (void)shift_y;
    (void)stream;
    // TODO: port 8-bit sub_frame
}

void binary16(const uint16_t* src, uint8_t* dst, uint16_t threshold, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_binary16<<<grid, block, 0, stream>>>(src, dst, threshold, n);
}

void reduce_large_dn16(const uint16_t* src, uint16_t* dst, float thresh, int n,
                       cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_reduce_large_dn16<<<grid, block, 0, stream>>>(src, dst, thresh, n);
}

void reduce_small_dn16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, int n,
                       cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_reduce_small_dn16<<<grid, block, 0, stream>>>(src, dst, avg, std_dev, n);
}

void reduce_saturation16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, float ratio,
                         int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_reduce_saturation16<<<grid, block, 0, stream>>>(src, dst, avg, std_dev, ratio, n);
}

void reduce_saturation8(const uint8_t* src, uint8_t* dst, float avg, float std_dev, float ratio,
                        int n, cudaStream_t stream) {
    (void)src;
    (void)dst;
    (void)avg;
    (void)std_dev;
    (void)ratio;
    (void)n;
    (void)stream;
    // TODO: port 8-bit variant
}

}  // namespace Dss::Gpu
