#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__global__ void kernel_gaussian16(const uint16_t* in, uint16_t* out, int w, int h,
                                  const float* kernel_data, int ksize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    int half = ksize / 2;
    float sum = 0.0f;
    for (int ky = -half; ky <= half; ++ky) {
        for (int kx = -half; kx <= half; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            float weight = kernel_data[(ky + half) * ksize + (kx + half)];
            sum += weight * static_cast<float>(in[sy * w + sx]);
        }
    }
    out[y * w + x] = static_cast<uint16_t>(max(0.0f, min(65535.0f, sum)));
}

__global__ void kernel_gaussian8(const uint8_t* in, uint8_t* out, int w, int h,
                                 const float* kernel_data, int ksize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    int half = ksize / 2;
    float sum = 0.0f;
    for (int ky = -half; ky <= half; ++ky) {
        for (int kx = -half; kx <= half; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            float weight = kernel_data[(ky + half) * ksize + (kx + half)];
            sum += weight * static_cast<float>(in[sy * w + sx]);
        }
    }
    out[y * w + x] = static_cast<uint8_t>(max(0.0f, min(255.0f, sum)));
}

__global__ void kernel_median16(const uint16_t* in, uint16_t* out, int w, int h, int ksize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    int half = ksize / 2;
    uint16_t vals[49];  // max 7x7
    int count = 0;

    for (int ky = -half; ky <= half; ++ky) {
        for (int kx = -half; kx <= half; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            if (count < 49) {
                vals[count++] = in[sy * w + sx];
            }
        }
    }

    // Partial sort to find median
    for (int i = 0; i <= count / 2; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (vals[j] < vals[i]) {
                uint16_t tmp = vals[i];
                vals[i] = vals[j];
                vals[j] = tmp;
            }
        }
    }
    out[y * w + x] = vals[count / 2];
}

__global__ void kernel_gray_dilate16(const uint16_t* in, uint16_t* out, int w, int h, int hsize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    uint16_t max_val = 0;
    for (int ky = -hsize; ky <= hsize; ++ky) {
        for (int kx = -hsize; kx <= hsize; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            max_val = max(max_val, in[sy * w + sx]);
        }
    }
    out[y * w + x] = max_val;
}

__global__ void kernel_gray_dilate8(const uint8_t* in, uint8_t* out, int w, int h, int hsize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    uint8_t max_val = 0;
    for (int ky = -hsize; ky <= hsize; ++ky) {
        for (int kx = -hsize; kx <= hsize; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            max_val = max(max_val, in[sy * w + sx]);
        }
    }
    out[y * w + x] = max_val;
}

__global__ void kernel_gray_erode16(const uint16_t* in, uint16_t* out, int w, int h, int hsize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    uint16_t min_val = 65535;
    for (int ky = -hsize; ky <= hsize; ++ky) {
        for (int kx = -hsize; kx <= hsize; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            min_val = min(min_val, in[sy * w + sx]);
        }
    }
    out[y * w + x] = min_val;
}

__global__ void kernel_bw_erode8(const uint8_t* in, uint8_t* out, int w, int h, int hsize) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) {
        return;
    }

    uint8_t val = 255;
    for (int ky = -hsize; ky <= hsize; ++ky) {
        for (int kx = -hsize; kx <= hsize; ++kx) {
            int sx = min(max(x + kx, 0), w - 1);
            int sy = min(max(y + ky, 0), h - 1);
            val = min(val, in[sy * w + sx]);
        }
    }
    out[y * w + x] = val;
}

// --- Host wrappers ---

static dim3 grid2d(int w, int h, int bx = 16, int by = 16) {
    return dim3((w + bx - 1) / bx, (h + by - 1) / by);
}

void gaussian_filter16(const uint16_t* in, uint16_t* out, int w, int h, const float* kernel_data,
                       int ksize, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_gaussian16<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, kernel_data, ksize);
}

void gaussian_filter8(const uint8_t* in, uint8_t* out, int w, int h, const float* kernel_data,
                      int ksize, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_gaussian8<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, kernel_data, ksize);
}

void median_filter16(const uint16_t* in, uint16_t* out, int w, int h, int ksize,
                     cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_median16<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, ksize);
}

void salt_filter8(const uint8_t* in, uint8_t* out, int w, int h, int ksize, cudaStream_t stream) {
    (void)in;
    (void)out;
    (void)w;
    (void)h;
    (void)ksize;
    (void)stream;
    // TODO: port salt filter (outlier replacement)
}

void gray_dilate16(const uint16_t* in, uint16_t* out, int w, int h, int hsize,
                   cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_gray_dilate16<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, hsize);
}

void gray_dilate8(const uint8_t* in, uint8_t* out, int w, int h, int hsize, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_gray_dilate8<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, hsize);
}

void gray_erode16(const uint16_t* in, uint16_t* out, int w, int h, int hsize, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_gray_erode16<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, hsize);
}

void bw_erode8(const uint8_t* in, uint8_t* out, int w, int h, int hsize, cudaStream_t stream) {
    dim3 block(16, 16);
    kernel_bw_erode8<<<grid2d(w, h), block, 0, stream>>>(in, out, w, h, hsize);
}

}  // namespace Dss::Gpu
