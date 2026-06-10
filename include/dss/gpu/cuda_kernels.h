#pragma once

#include <cstdint>

#include <cuda_runtime.h>

namespace Dss::Gpu {

/// @name statistics.cu — 统计运算
/// @{

/**
 * @brief 计算 16 位图像的像素直方图
 * @param image 输入图像（设备端）
 * @param histogram 输出直方图缓冲区，长度至少 65536（设备端）
 * @param n 像素总数
 * @param stream 可选 CUDA 流
 */
void hist16(const uint16_t* image, uint32_t* histogram, int n, cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位数据按组求和（用于后续计算分组均值）
 * @param src 输入数据（设备端）
 * @param out 每组求和结果，长度为 groups（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void avg_group_sum16(const uint16_t* src, double* out, int n, int groups,
                     cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位数据按组求和（用于后续计算分组均值）
 * @param src 输入数据（设备端）
 * @param out 每组求和结果，长度为 groups（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void avg_group_sum8(const uint8_t* src, double* out, int n, int groups,
                    cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位数据按组计算方差相关求和
 * @param src 输入数据（设备端）
 * @param avg 全局或参考均值
 * @param out 每组方差求和结果（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void var_group_sum16(const uint16_t* src, double avg, double* out, int n, int groups,
                     cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位数据按组计算方差相关求和
 * @param src 输入数据（设备端）
 * @param avg 全局或参考均值
 * @param out 每组方差求和结果（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void var_group_sum8(const uint8_t* src, double avg, double* out, int n, int groups,
                    cudaStream_t stream = nullptr);

/**
 * @brief 对数组执行全局求和归约
 * @param data 输入/输出数组，归约结果写入 data[0]（设备端）
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void global_sum(double* data, int n, cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位数据按组计算最小值与最大值
 * @param src 输入数据（设备端）
 * @param gmin 每组最小值输出（设备端）
 * @param gmax 每组最大值输出（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void min_max_group16(const uint16_t* src, double* gmin, double* gmax, int n, int groups,
                     cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位数据按组计算最小值与最大值
 * @param src 输入数据（设备端）
 * @param gmin 每组最小值输出（设备端）
 * @param gmax 每组最大值输出（设备端）
 * @param n 元素总数
 * @param groups 分组数量
 * @param stream 可选 CUDA 流
 */
void min_max_group8(const uint8_t* src, double* gmin, double* gmax, int n, int groups,
                    cudaStream_t stream = nullptr);

/**
 * @brief 对数组执行全局最小/最大值归约
 * @param gmin 最小值输出，结果写入 gmin[0]（设备端）
 * @param gmax 最大值输出，结果写入 gmax[0]（设备端）
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void global_min_max(double* gmin, double* gmax, int n, cudaStream_t stream = nullptr);

/// @}

/// @name filter.cu — 滤波与形态学运算
/// @{

/**
 * @brief 对 16 位图像应用高斯卷积滤波
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param kernel_data 卷积核系数（设备端）
 * @param ksize 卷积核边长（奇数）
 * @param stream 可选 CUDA 流
 */
void gaussian_filter16(const uint16_t* in, uint16_t* out, int w, int h, const float* kernel_data,
                       int ksize, cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位图像应用高斯卷积滤波
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param kernel_data 卷积核系数（设备端）
 * @param ksize 卷积核边长（奇数）
 * @param stream 可选 CUDA 流
 */
void gaussian_filter8(const uint8_t* in, uint8_t* out, int w, int h, const float* kernel_data,
                      int ksize, cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位图像应用中值滤波
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param ksize 滤波窗口边长（奇数）
 * @param stream 可选 CUDA 流
 */
void median_filter16(const uint16_t* in, uint16_t* out, int w, int h, int ksize,
                     cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位图像应用椒盐噪声（离群点）滤波
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param ksize 滤波窗口边长
 * @param stream 可选 CUDA 流
 */
void salt_filter8(const uint8_t* in, uint8_t* out, int w, int h, int ksize,
                  cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位灰度图像执行形态学膨胀
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param hsize 结构元素半宽
 * @param stream 可选 CUDA 流
 */
void gray_dilate16(const uint16_t* in, uint16_t* out, int w, int h, int hsize,
                   cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位灰度图像执行形态学膨胀
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param hsize 结构元素半宽
 * @param stream 可选 CUDA 流
 */
void gray_dilate8(const uint8_t* in, uint8_t* out, int w, int h, int hsize,
                  cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位灰度图像执行形态学腐蚀
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param hsize 结构元素半宽
 * @param stream 可选 CUDA 流
 */
void gray_erode16(const uint16_t* in, uint16_t* out, int w, int h, int hsize,
                  cudaStream_t stream = nullptr);

/**
 * @brief 对 8 位二值图像执行形态学腐蚀
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param hsize 结构元素半宽
 * @param stream 可选 CUDA 流
 */
void bw_erode8(const uint8_t* in, uint8_t* out, int w, int h, int hsize,
               cudaStream_t stream = nullptr);

/// @}

/// @name transform.cu — 几何变换
/// @{

/**
 * @brief 对 16 位图像绕指定中心旋转
 * @param in 输入图像（设备端）
 * @param out 输出图像（设备端）
 * @param fout 可选浮点输出缓冲区，与 out 同尺寸（设备端）；可为 nullptr
 * @param w 图像宽度
 * @param h 图像高度
 * @param cx 旋转中心 x 坐标
 * @param cy 旋转中心 y 坐标
 * @param sin_a 旋转角正弦值
 * @param cos_a 旋转角余弦值
 * @param stream 可选 CUDA 流
 */
void rotate16(const uint16_t* in, uint16_t* out, float* fout, int w, int h, float cx, float cy,
              float sin_a, float cos_a, cudaStream_t stream = nullptr);

/**
 * @brief 裁剪 16 位图像的指定边距区域
 * @param src 原始图像（设备端）
 * @param dst 裁剪后图像（设备端）
 * @param w_ori 原始宽度
 * @param h_ori 原始高度
 * @param left 左侧裁剪像素数
 * @param right 右侧裁剪像素数
 * @param top 顶部裁剪像素数
 * @param bottom 底部裁剪像素数
 * @param stream 可选 CUDA 流
 */
void crop16(const uint16_t* src, uint16_t* dst, int w_ori, int h_ori, int left, int right, int top,
            int bottom, cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位图像执行像素合并（binning）降采样
 * @param in 输入图像（设备端）
 * @param out 输出图像，尺寸为 w/bin × h/bin（设备端）
 * @param w 输入宽度（须能被 bin 整除）
 * @param h 输入高度（须能被 bin 整除）
 * @param bin 合并块边长
 * @param stream 可选 CUDA 流
 */
void binning16(const uint16_t* in, uint16_t* out, int w, int h, int bin,
               cudaStream_t stream = nullptr);

/**
 * @brief 将 16 位图像按行优先转为列优先布局（patch）
 * @param src 行优先输入（设备端）
 * @param dst 列优先输出（设备端）
 * @param r 行数
 * @param c 列数
 * @param n 像素总数，须等于 r × c
 * @param stream 可选 CUDA 流
 */
void patch16(const uint16_t* src, uint16_t* dst, int r, int c, int n,
             cudaStream_t stream = nullptr);

/**
 * @brief 将 8 位列优先布局还原为行优先（depatch）
 * @param src 列优先输入（设备端）
 * @param dst 行优先输出（设备端）
 * @param r 行数
 * @param c 列数
 * @param n 像素总数，须等于 r × c
 * @param stream 可选 CUDA 流
 */
void depatch8(const uint8_t* src, uint8_t* dst, int r, int c, int n, cudaStream_t stream = nullptr);

/**
 * @brief 将 16 位列优先布局还原为行优先（depatch）
 * @param src 列优先输入（设备端）
 * @param dst 行优先输出（设备端）
 * @param r 行数
 * @param c 列数
 * @param n 像素总数，须等于 r × c
 * @param stream 可选 CUDA 流
 */
void depatch16(const uint16_t* src, uint16_t* dst, int r, int c, int n,
               cudaStream_t stream = nullptr);

/// @}

/// @name arithmetic.cu — 算术与阈值运算
/// @{

/**
 * @brief 计算两帧 16 位图像的差分（支持位移对齐）
 * @param src1 被减帧（设备端）
 * @param src2 减数帧（设备端）
 * @param dst 差分结果，负值截断为 0（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param shift_x src2 相对 src1 的 x 方向偏移
 * @param shift_y src2 相对 src1 的 y 方向偏移
 * @param stream 可选 CUDA 流
 */
void sub_frame16(const uint16_t* src1, const uint16_t* src2, uint16_t* dst, int w, int h,
                 int shift_x, int shift_y, cudaStream_t stream = nullptr);

/**
 * @brief 计算两帧 8 位图像的差分（支持位移对齐）
 * @param src1 被减帧（设备端）
 * @param src2 减数帧（设备端）
 * @param dst 差分结果（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param shift_x src2 相对 src1 的 x 方向偏移
 * @param shift_y src2 相对 src1 的 y 方向偏移
 * @param stream 可选 CUDA 流
 */
void sub_frame8(const uint8_t* src1, const uint8_t* src2, uint8_t* dst, int w, int h, int shift_x,
                int shift_y, cudaStream_t stream = nullptr);

/**
 * @brief 对 16 位数据执行阈值二值化
 * @param src 输入数据（设备端）
 * @param dst 输出 8 位二值图，大于阈值为 255，否则为 0（设备端）
 * @param threshold 阈值
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void binary16(const uint16_t* src, uint8_t* dst, uint16_t threshold, int n,
              cudaStream_t stream = nullptr);

/**
 * @brief 将 16 位数据线性映射为 8 位灰度
 * @param src 输入数据（设备端）
 * @param dst 输出 8 位数据（设备端）
 * @param min_val 映射下限，对应输出 0
 * @param max_val 映射上限，对应输出 255
 * @param diff 有效区间宽度，通常为 max_val - min_val
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void short_to_byte(const uint16_t* src, uint8_t* dst, int min_val, int max_val, float diff, int n,
                   cudaStream_t stream = nullptr);

/**
 * @brief 抑制 16 位图像中的大像元噪声（高于阈值置零）
 * @param src 输入数据（设备端）
 * @param dst 输出数据（设备端）
 * @param thresh 噪声阈值
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void reduce_large_dn16(const uint16_t* src, uint16_t* dst, float thresh, int n,
                       cudaStream_t stream = nullptr);

/**
 * @brief 抑制 16 位图像中的小像元噪声（低于均值减 3σ 替换为均值）
 * @param src 输入数据（设备端）
 * @param dst 输出数据（设备端）
 * @param avg 参考均值
 * @param std_dev 参考标准差
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void reduce_small_dn16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, int n,
                       cudaStream_t stream = nullptr);

/**
 * @brief 抑制 16 位图像中的饱和像素（高于均值加 ratio×σ 替换为均值）
 * @param src 输入数据（设备端）
 * @param dst 输出数据（设备端）
 * @param avg 参考均值
 * @param std_dev 参考标准差
 * @param ratio 标准差倍数系数
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void reduce_saturation16(const uint16_t* src, uint16_t* dst, float avg, float std_dev, float ratio,
                         int n, cudaStream_t stream = nullptr);

/**
 * @brief 抑制 8 位图像中的饱和像素（高于均值加 ratio×σ 替换为均值）
 * @param src 输入数据（设备端）
 * @param dst 输出数据（设备端）
 * @param avg 参考均值
 * @param std_dev 参考标准差
 * @param ratio 标准差倍数系数
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void reduce_saturation8(const uint8_t* src, uint8_t* dst, float avg, float std_dev, float ratio,
                        int n, cudaStream_t stream = nullptr);

/// @}

/// @name calibration.cu — 标定与校正
/// @{

/**
 * @brief 检测并修复图像中的坏线/坏点
 * @param in 输入图像（设备端）
 * @param out 校正后图像（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param detect_w 检测区域宽度
 * @param detect_h 检测区域高度
 * @param threshold_x 行方向偏差阈值
 * @param threshold_y 列方向偏差阈值
 * @param stream 可选 CUDA 流
 */
void remove_bad_line(const uint16_t* in, uint16_t* out, int w, int h, int detect_w, int detect_h,
                     float threshold_x, float threshold_y, cudaStream_t stream = nullptr);

/**
 * @brief 使用平场与偏置图对 16 位数据进行辐射校正
 * @param in 输入数据（设备端）
 * @param out 校正后数据（设备端）
 * @param flat 平场图（设备端）
 * @param bias 偏置图（设备端）
 * @param flat_avg 平场参考均值
 * @param flat_std 平场参考标准差
 * @param n 元素个数
 * @param stream 可选 CUDA 流
 */
void flat_bias_correct(const uint16_t* in, uint16_t* out, const uint16_t* flat,
                       const uint16_t* bias, float flat_avg, float flat_std, int n,
                       cudaStream_t stream = nullptr);

/**
 * @brief 局部对比度增强（Local Contrast Modification）
 * @param in 输入图像（设备端）
 * @param out 输出图像，结果为像素值减去局部均值（设备端）
 * @param w 图像宽度
 * @param h 图像高度
 * @param box_size 局部窗口边长
 * @param stream 可选 CUDA 流
 */
void lcm(const uint16_t* in, uint16_t* out, int w, int h, int box_size,
         cudaStream_t stream = nullptr);

/// @}

/// @name composite.cu — 多帧合成
/// @{

/**
 * @brief 对 5 帧 16 位图像逐像素取中值合成
 * @param s1 第 1 帧（设备端）
 * @param s2 第 2 帧（设备端）
 * @param s3 第 3 帧（设备端）
 * @param s4 第 4 帧（设备端）
 * @param s5 第 5 帧（设备端）
 * @param out 合成结果（设备端）
 * @param n 像素总数
 * @param stream 可选 CUDA 流
 */
void frame5_median16(const uint16_t* s1, const uint16_t* s2, const uint16_t* s3, const uint16_t* s4,
                     const uint16_t* s5, uint16_t* out, int n, cudaStream_t stream = nullptr);

/// @}

}  // namespace Dss::Gpu
