#pragma once

#include <cuda_runtime.h>

#include <cstdint>

namespace Dss::Gpu
{

// --- statistics.cu ---
void hist16(const uint16_t* image, uint32_t* histogram, int n, cudaStream_t stream = nullptr);
void avg_group_sum16(const uint16_t* src, double* out, int n, int groups, cudaStream_t stream = nullptr);
void avg_group_sum8(const uint8_t* src, double* out, int n, int groups, cudaStream_t stream = nullptr);
void var_group_sum16(const uint16_t* src, double avg, double* out, int n, int groups, cudaStream_t stream = nullptr);
void var_group_sum8(const uint8_t* src, double avg, double* out, int n, int groups, cudaStream_t stream = nullptr);
void global_sum(double* data, int n, cudaStream_t stream = nullptr);
void min_max_group16(const uint16_t* src, double* gmin, double* gmax, int n, int groups, cudaStream_t stream = nullptr);
void min_max_group8(const uint8_t* src, double* gmin, double* gmax, int n, int groups, cudaStream_t stream = nullptr);
void global_min_max(double* gmin, double* gmax, int n, cudaStream_t stream = nullptr);

// --- filter.cu ---
void gaussian_filter16(const uint16_t* in, uint16_t* out, int w, int h, const float* kernel_data, int ksize, cudaStream_t stream = nullptr);
void gaussian_filter8(const uint8_t* in, uint8_t* out, int w, int h, const float* kernel_data, int ksize, cudaStream_t stream = nullptr);
void median_filter16(const uint16_t* in, uint16_t* out, int w, int h, int ksize, cudaStream_t stream = nullptr);
void salt_filter8(const uint8_t* in, uint8_t* out, int w, int h, int ksize, cudaStream_t stream = nullptr);
void gray_dilate16(const uint16_t* in, uint16_t* out, int w, int h, int hsize, cudaStream_t stream = nullptr);
void gray_dilate8(const uint8_t* in, uint8_t* out, int w, int h, int hsize, cudaStream_t stream = nullptr);
void gray_erode16(const uint16_t* in, uint16_t* out, int w, int h, int hsize, cudaStream_t stream = nullptr);
void bw_erode8(const uint8_t* in, uint8_t* out, int w, int h, int hsize, cudaStream_t stream = nullptr);

// --- transform.cu ---
void rotate16(const uint16_t* in, uint16_t* out, float* fout, int w, int h, float cx, float cy, float sin_a, float cos_a, cudaStream_t stream = nullptr);
void crop16(const uint16_t* src, uint16_t* dst, int w_ori, int h_ori, int left, int right, int top, int bottom, cudaStream_t stream = nullptr);
void binning16(const uint16_t* in, uint16_t* out, int w, int h, int bin, cudaStream_t stream = nullptr);
void patch16(const uint16_t* src, uint16_t* dst, int r, int c, int n, cudaStream_t stream = nullptr);
void depatch8(const uint8_t* src, uint8_t* dst, int r, int c, int n, cudaStream_t stream = nullptr);
void depatch16(const uint16_t* src, uint16_t* dst, int r, int c, int n, cudaStream_t stream = nullptr);

// --- arithmetic.cu ---
void sub_frame16(const uint16_t* src1, const uint16_t* src2, uint16_t* dst, int w, int h, int shift_x, int shift_y, cudaStream_t stream = nullptr);
void sub_frame8(const uint8_t* src1, const uint8_t* src2, uint8_t* dst, int w, int h, int shift_x, int shift_y, cudaStream_t stream = nullptr);
void binary16(const uint16_t* src, uint8_t* dst, uint16_t threshold, int n, cudaStream_t stream = nullptr);
void short_to_byte(const uint16_t* src, uint8_t* dst, int min_val, int max_val, float diff, int n, cudaStream_t stream = nullptr);
void reduce_large_dn16(const uint16_t* src, uint16_t* dst, float thresh, int n, cudaStream_t stream = nullptr);
void reduce_small_dn16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, int n, cudaStream_t stream = nullptr);
void reduce_saturation16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, float ratio, int n, cudaStream_t stream = nullptr);
void reduce_saturation8(const uint8_t* src, uint8_t* dst, float avg, float std_dev, float ratio, int n, cudaStream_t stream = nullptr);

// --- calibration.cu ---
void remove_bad_line(const uint16_t* in, uint16_t* out, int w, int h, int detect_w, int detect_h, float threshold_x, float threshold_y, cudaStream_t stream = nullptr);
void flat_bias_correct(const uint16_t* in, uint16_t* out, const uint16_t* flat, const uint16_t* bias, float flat_avg, float flat_std, int n, cudaStream_t stream = nullptr);
void lcm(const uint16_t* in, uint16_t* out, int w, int h, int box_size, cudaStream_t stream = nullptr);

// --- composite.cu ---
void frame5_median16(const uint16_t* s1, const uint16_t* s2, const uint16_t* s3, const uint16_t* s4, const uint16_t* s5, uint16_t* out, int n, cudaStream_t stream = nullptr);

} // namespace Dss::Gpu
