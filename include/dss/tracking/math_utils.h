#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace Dss::Math {

/// 多项式拟合结果
struct PolynomialFitResult {
    std::vector<double> coefficients;  ///< 多项式系数（低次到高次）
    std::vector<double> fittedY;       ///< 各采样点的拟合值
    double ssr = 0.0;                  ///< 回归平方和
    double sse = 0.0;                  ///< 残差平方和
    double rmse = 0.0;                   ///< 均方根误差
    double rSquared = 0.0;               ///< 决定系数 R²
};

/// 采样周期估算结果
struct SamplePeriodEstimate {
    double period = 0.0;    ///< 估算的采样周期
    double maxError = 0.0;  ///< 该周期下的最大误差
};

/// 去趋势后的时间序列
struct DetrendedSeries {
    std::vector<double> coefficients;  ///< 趋势多项式系数
    std::vector<double> fittedY;       ///< 趋势拟合值
    std::vector<double> residuals;     ///< 去趋势后的残差序列
};

/// 兼容旧版实现的 FFT 频谱结果
struct LegacyFftSpectrum {
    std::vector<double> real;          ///< 频域实部
    std::vector<double> imag;          ///< 频域虚部
    std::vector<double> amplitude;     ///< 各频率 bin 的幅值
    std::vector<double> phaseDegrees;  ///< 各频率 bin 的相位（度）
    double baseFrequency = 0.0;        ///< 频率 bin 间隔（Hz）
    double peakAmplitude = 0.0;        ///< 峰值幅值
    std::size_t fftSize = 0;           ///< FFT 点数
};

/**
 * @brief 对数据点执行多项式最小二乘拟合，仅返回系数
 * @param x 自变量序列
 * @param y 因变量序列
 * @param order 多项式阶数
 * @return 拟合系数；失败时返回空向量
 */
auto polyfit(std::span<const double> x, std::span<const double> y, int order)
    -> std::vector<double>;

/**
 * @brief 对数据点执行多项式最小二乘拟合，返回完整统计量
 * @param x 自变量序列
 * @param y 因变量序列
 * @param order 多项式阶数
 * @param saveFittedY 是否保存各点拟合值
 * @return 拟合结果；输入无效时返回 nullopt
 */
auto polynomialFit(std::span<const double> x, std::span<const double> y, int order,
                   bool saveFittedY = true) -> std::optional<PolynomialFitResult>;

/**
 * @brief 对数据点执行一阶线性拟合
 * @param x 自变量序列
 * @param y 因变量序列
 * @param saveFittedY 是否保存各点拟合值
 * @return 拟合结果；输入无效时返回 nullopt
 */
auto linearFit(std::span<const double> x, std::span<const double> y, bool saveFittedY = true)
    -> std::optional<PolynomialFitResult>;

/**
 * @brief 评估给定采样周期下时间戳序列的最大周期性误差
 * @param samples 时间戳或采样时刻序列
 * @param samplePeriod 待评估的采样周期
 * @return 最大误差；输入无效时返回 nullopt
 */
auto samplePeriodError(std::span<const double> samples, double samplePeriod)
    -> std::optional<double>;

/**
 * @brief 从时间戳序列估算最小有效采样周期
 * @param samples 时间戳序列
 * @param errorLimitRate 允许的最大相对误差率
 * @return 周期估算结果；无法收敛时返回 nullopt
 */
auto minimumSamplePeriod(std::span<const double> samples, double errorLimitRate = 0.2)
    -> std::optional<SamplePeriodEstimate>;

/**
 * @brief 去除时间序列中的多项式趋势，返回残差
 * @param x 自变量序列
 * @param y 因变量序列
 * @param order 趋势多项式阶数
 * @return 去趋势结果；拟合失败时返回 nullopt
 */
auto removePolynomialTrend(std::span<const double> x, std::span<const double> y, int order)
    -> std::optional<DetrendedSeries>;

/**
 * @brief 最近邻采样插值（兼容旧版实现）
 * @param x 已知采样点的自变量
 * @param y 已知采样点的因变量
 * @param targetX 待插值的自变量
 * @return 插值结果；输入无效时返回 nullopt
 */
auto legacyNearestSampleInterpolate(std::span<const double> x, std::span<const double> y,
                                    std::span<const double> targetX)
    -> std::optional<std::vector<double>>;

/**
 * @brief 根据样本数量确定兼容旧版的 FFT 点数（2 的幂）
 * @param sampleCount 原始样本数
 * @return FFT 点数；无效输入时返回 nullopt
 */
auto legacyFftSize(std::size_t sampleCount) -> std::optional<std::size_t>;

/**
 * @brief 对实信号执行 FFT 并计算频谱（兼容旧版实现）
 * @param real 时域实部
 * @param sampleInterval 采样间隔（秒）
 * @return 频谱结果；输入无效时返回 nullopt
 */
auto legacyFftSpectrum(std::span<const double> real, double sampleInterval)
    -> std::optional<LegacyFftSpectrum>;

/**
 * @brief 对复信号执行 FFT 并计算频谱（兼容旧版实现）
 * @param real 时域实部
 * @param imag 时域虚部
 * @param sampleInterval 采样间隔（秒）
 * @return 频谱结果；输入无效时返回 nullopt
 */
auto legacyFftSpectrum(std::span<const double> real, std::span<const double> imag,
                       double sampleInterval) -> std::optional<LegacyFftSpectrum>;

/**
 * @brief 计算多项式在指定点的值
 * @param coeffs 多项式系数（低次到高次）
 * @param x 自变量
 * @return 多项式值
 */
auto polyval(std::span<const double> coeffs, double x) -> double;

/**
 * @brief 对实信号执行离散傅里叶变换
 * @param input 时域实信号
 * @param outputReal 频域实部输出
 * @param outputImag 频域虚部输出
 */
void fft(std::span<const double> input, std::span<double> outputReal, std::span<double> outputImag);

/**
 * @brief 对复频谱执行逆离散傅里叶变换
 * @param inputReal 频域实部
 * @param inputImag 频域虚部
 * @param output 时域实信号输出
 */
void ifft(std::span<const double> inputReal, std::span<const double> inputImag,
          std::span<double> output);

/**
 * @brief 通过 FFT 主频峰值估算信号周期
 * @param signal 时域信号
 * @param sampleRate 采样率（Hz）
 * @return 估算周期（秒）；无法估算时返回 0
 */
auto estimatePeriod(std::span<const double> signal, double sampleRate) -> double;

/**
 * @brief 计算浮点序列的中位数
 * @param values 输入序列
 * @return 中位数值；空序列返回 0
 */
auto median(std::span<const float> values) -> float;

}  // namespace Dss::Math
