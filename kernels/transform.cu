#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__global__ void kernel_rotate16(const uint16_t* in, uint16_t* out, float* fout, int w, int h,
                                float cx, float cy, float sin_a, float cos_a) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    float fx = static_cast<float>(x) - cx;
    float fy = static_cast<float>(y) - cy;
    float src_x = fx * cos_a + fy * sin_a + cx;
    float src_y = -fx * sin_a + fy * cos_a + cy;

    int sx = static_cast<int>(src_x + 0.5f);
    int sy = static_cast<int>(src_y + 0.5f);

    int dst_idx = y * w + x;
    if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
        out[dst_idx] = in[sy * w + sx];
        if (fout) {
            fout[dst_idx] = static_cast<float>(in[sy * w + sx]);
        }
    } else {
        out[dst_idx] = 0;
        if (fout) {
            fout[dst_idx] = 0.0f;
        }
    }
}

__global__ void kernel_crop16(const uint16_t* src, uint16_t* dst, int w_ori, int h_ori, int left,
                              int right, int top, int bottom) {
    int w_new = w_ori - left - right;
    int h_new = h_ori - top - bottom;

    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w_new || y >= h_new) {
        return;
    }

    dst[y * w_new + x] = src[(y + top) * w_ori + (x + left)];
}

__global__ void kernel_binning16(const uint16_t* in, uint16_t* out, int w, int h, int bin) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int w_out = w / bin;
    int h_out = h / bin;
    if (x >= w_out || y >= h_out) {
        return;
    }

    float sum = 0.0f;
    for (int by = 0; by < bin; ++by) {
        for (int bx = 0; bx < bin; ++bx) {
            int sx = x * bin + bx;
            int sy = y * bin + by;
            if (sx < w && sy < h) {
                sum += static_cast<float>(in[sy * w + sx]);
            }
        }
    }
    out[y * w_out + x] = static_cast<uint16_t>(sum / static_cast<float>(bin * bin));
}

__global__ void kernel_patch16(const uint16_t* src, uint16_t* dst, int r, int c) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int total = r * c;
    if (gid >= total) {
        return;
    }
    int y = gid / c;
    int x = gid % c;
    dst[x * r + y] = src[y * c + x];
}

__global__ void kernel_depatch8(const uint8_t* src, uint8_t* dst, int r, int c) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int total = r * c;
    if (gid >= total) {
        return;
    }
    int y = gid / c;
    int x = gid % c;
    dst[y * c + x] = src[x * r + y];
}

__global__ void kernel_depatch16(const uint16_t* src, uint16_t* dst, int r, int c) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int total = r * c;
    if (gid >= total) {
        return;
    }
    int y = gid / c;
    int x = gid % c;
    dst[y * c + x] = src[x * r + y];
}

// --- Host wrappers ---

static dim3 grid2d(int w, int h, int bx = 16, int by = 16) {
    return dim3((w + bx - 1) / bx, (h + by - 1) / by);
}

void rotate16(const uint16_t* in, uint16_t* out, float* fout, int w, int h, float cx, float cy,
              float sin_a, float cos_a, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_rotate16<<<grid2d(w, h), block, 0, stream>>>(in, out, fout, w, h, cx, cy, sin_a, cos_a);
}

void crop16(const uint16_t* src, uint16_t* dst, int w_ori, int h_ori, int left, int right, int top,
            int bottom, cudaStream_t stream) {
    int w_new = w_ori - left - right;
    int h_new = h_ori - top - bottom;
    dim3 block(16, 16);
    kernel_crop16<<<grid2d(w_new, h_new), block, 0, stream>>>(src, dst, w_ori, h_ori, left, right,
                                                              top, bottom);
}

void binning16(const uint16_t* in, uint16_t* out, int w, int h, int bin, cudaStream_t stream) {
    int w_out = w / bin;
    int h_out = h / bin;
    dim3 block(16, 16);
    kernel_binning16<<<grid2d(w_out, h_out), block, 0, stream>>>(in, out, w, h, bin);
}

void patch16(const uint16_t* src, uint16_t* dst, int r, int c, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_patch16<<<grid, block, 0, stream>>>(src, dst, r, c);
}

void depatch8(const uint8_t* src, uint8_t* dst, int r, int c, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_depatch8<<<grid, block, 0, stream>>>(src, dst, r, c);
}

void depatch16(const uint16_t* src, uint16_t* dst, int r, int c, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_depatch16<<<grid, block, 0, stream>>>(src, dst, r, c);
}

}  // namespace Dss::Gpu
