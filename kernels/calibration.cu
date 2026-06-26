#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__global__ void kernel_remove_bad_line(const uint16_t* in, uint16_t* out, int w, int h,
                                       int detect_w, int detect_h, float threshold_x,
                                       float threshold_y) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= detect_w || y >= detect_h) {
        return;
    }

    int src_x = x + (w - detect_w) / 2;
    int src_y = y + (h - detect_h) / 2;
    int idx = src_y * w + src_x;

    float val = static_cast<float>(in[idx]);

    // Column average check
    float col_avg = 0;
    for (int ky = -2; ky <= 2; ++ky) {
        int yy = min(max(src_y + ky, 0), h - 1);
        col_avg += static_cast<float>(in[yy * w + src_x]);
    }
    col_avg /= 5.0f;

    // Row average check
    float row_avg = 0;
    for (int kx = -2; kx <= 2; ++kx) {
        int xx = min(max(src_x + kx, 0), w - 1);
        row_avg += static_cast<float>(in[src_y * w + xx]);
    }
    row_avg /= 5.0f;

    if (fabsf(val - col_avg) > threshold_y || fabsf(val - row_avg) > threshold_x) {
        out[idx] = static_cast<uint16_t>(col_avg * 0.5f + row_avg * 0.5f);
    } else {
        out[idx] = in[idx];
    }
}

__global__ void kernel_flat_bias_correct(const uint16_t* in, uint16_t* out, const uint16_t* flat,
                                         const uint16_t* bias, float flat_avg, float flat_std,
                                         int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }

    float corrected = (static_cast<float>(in[gid]) - static_cast<float>(bias[gid])) * flat_avg /
                      fmaxf(1.0f, static_cast<float>(flat[gid]) - static_cast<float>(bias[gid]));
    out[gid] = static_cast<uint16_t>(fmaxf(0.0f, fminf(65535.0f, corrected)));
}

__global__ void kernel_lcm(const uint16_t* in, uint16_t* out, int w, int h, int box_size) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    int half = box_size / 2;
    float sum = 0.0f;
    int count = 0;

    for (int ky = -half; ky <= half; ++ky) {
        for (int kx = -half; kx <= half; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            sum += static_cast<float>(in[sy * w + sx]);
            ++count;
        }
    }

    float local_mean = sum / static_cast<float>(count);
    float val = static_cast<float>(in[y * w + x]);
    float result = val - local_mean;
    out[y * w + x] = static_cast<uint16_t>(fmaxf(0.0f, result));
}

// --- Host wrappers ---

static dim3 grid2d(int w, int h, int bx = 16, int by = 16) {
    return dim3((w + bx - 1) / bx, (h + by - 1) / by);
}

void remove_bad_line(const uint16_t* in, uint16_t* out, int w, int h, int detect_w, int detect_h,
                     float threshold_x, float threshold_y, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_remove_bad_line<<<grid2d(detect_w, detect_h), block, 0, stream>>>(
        in, out, w, h, detect_w, detect_h, threshold_x, threshold_y);
}

void flat_bias_correct(const uint16_t* in, uint16_t* out, const uint16_t* flat,
                       const uint16_t* bias, float flat_avg, float flat_std, int n,
                       cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_flat_bias_correct<<<grid, block, 0, stream>>>(in, out, flat, bias, flat_avg, flat_std,
                                                         n);
}

void lcm(const uint16_t* in, uint16_t* out, int w, int h, int box_size, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_lcm<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, box_size);
}

}  // namespace Dss::Gpu
