#include <cuda_runtime.h>

#include "dss/gpu/cuda_kernels.h"

namespace Dss::Gpu {

__device__ uint16_t median5(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t e) {
    // Sorting network for 5 elements to find median
    auto swap = [](uint16_t& x, uint16_t& y) {
        if (x > y) {
            uint16_t tmp = x;
            x = y;
            y = tmp;
        }
    };

    swap(a, b);
    swap(c, d);
    swap(a, c);
    swap(b, d);
    swap(b, c);
    swap(a, e);
    swap(b, e);
    swap(c, e);

    return c;  // median is in position 2
}

__global__ void kernel_frame5_median16(const uint16_t* s1, const uint16_t* s2, const uint16_t* s3,
                                       const uint16_t* s4, const uint16_t* s5, uint16_t* out,
                                       int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) {
        return;
    }

    out[gid] = median5(s1[gid], s2[gid], s3[gid], s4[gid], s5[gid]);
}

// --- Host wrapper ---

void frame5_median16(const uint16_t* s1, const uint16_t* s2, const uint16_t* s3, const uint16_t* s4,
                     const uint16_t* s5, uint16_t* out, int n, cudaStream_t stream) {
    int block = 256;
    int grid = (n + block - 1) / block;
    kernel_frame5_median16<<<grid, block, 0, stream>>>(s1, s2, s3, s4, s5, out, n);
}

}  // namespace Dss::Gpu
