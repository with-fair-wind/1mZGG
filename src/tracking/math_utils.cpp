#include "dss/tracking/math_utils.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace Dss::Math
{

auto polyfit(std::span<const double> x, std::span<const double> y, int order) -> std::vector<double>
{
    const auto n = static_cast<int>(x.size());
    const auto m = order + 1;

    std::vector<double> A(m * m, 0.0);
    std::vector<double> b(m, 0.0);

    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < m; ++j)
        {
            for (int k = 0; k < n; ++k)
            {
                A[i * m + j] += std::pow(x[k], i + j);
            }
        }
        for (int k = 0; k < n; ++k)
        {
            b[i] += y[k] * std::pow(x[k], i);
        }
    }

    for (int i = 0; i < m; ++i)
    {
        int maxRow = i;
        for (int k = i + 1; k < m; ++k)
        {
            if (std::abs(A[k * m + i]) > std::abs(A[maxRow * m + i]))
            {
                maxRow = k;
            }
        }
        for (int k = 0; k < m; ++k)
        {
            std::swap(A[i * m + k], A[maxRow * m + k]);
        }
        std::swap(b[i], b[maxRow]);

        for (int k = i + 1; k < m; ++k)
        {
            double factor = A[k * m + i] / A[i * m + i];
            for (int j = i; j < m; ++j)
            {
                A[k * m + j] -= factor * A[i * m + j];
            }
            b[k] -= factor * b[i];
        }
    }

    std::vector<double> coeffs(m, 0.0);
    for (int i = m - 1; i >= 0; --i)
    {
        coeffs[i] = b[i];
        for (int j = i + 1; j < m; ++j)
        {
            coeffs[i] -= A[i * m + j] * coeffs[j];
        }
        coeffs[i] /= A[i * m + i];
    }

    return coeffs;
}

auto polyval(std::span<const double> coeffs, double x) -> double
{
    double result = 0.0;
    double xn = 1.0;
    for (double c : coeffs)
    {
        result += c * xn;
        xn *= x;
    }
    return result;
}

void fft(std::span<const double> input, std::span<double> outputReal, std::span<double> outputImag)
{
    const auto n = input.size();
    for (size_t k = 0; k < n; ++k)
    {
        double sumReal = 0.0;
        double sumImag = 0.0;
        for (size_t t = 0; t < n; ++t)
        {
            double angle = 2.0 * std::numbers::pi * static_cast<double>(k * t) / static_cast<double>(n);
            sumReal += input[t] * std::cos(angle);
            sumImag -= input[t] * std::sin(angle);
        }
        outputReal[k] = sumReal;
        outputImag[k] = sumImag;
    }
}

void ifft(std::span<const double> inputReal,
          std::span<const double> inputImag,
          std::span<double> output)
{
    const auto n = inputReal.size();
    for (size_t t = 0; t < n; ++t)
    {
        double sum = 0.0;
        for (size_t k = 0; k < n; ++k)
        {
            double angle = 2.0 * std::numbers::pi * static_cast<double>(k * t) / static_cast<double>(n);
            sum += inputReal[k] * std::cos(angle) - inputImag[k] * std::sin(angle);
        }
        output[t] = sum / static_cast<double>(n);
    }
}

auto estimatePeriod(std::span<const double> signal, double sampleRate) -> double
{
    const auto n = signal.size();
    std::vector<double> real(n);
    std::vector<double> imag(n);
    fft(signal, real, imag);

    double maxMagnitude = 0.0;
    size_t maxIndex = 1;
    for (size_t k = 1; k < n / 2; ++k)
    {
        double mag = std::sqrt(real[k] * real[k] + imag[k] * imag[k]);
        if (mag > maxMagnitude)
        {
            maxMagnitude = mag;
            maxIndex = k;
        }
    }

    double freq = static_cast<double>(maxIndex) * sampleRate / static_cast<double>(n);
    return (freq > 0.0) ? 1.0 / freq : 0.0;
}

auto median(std::span<const float> values) -> float
{
    if (values.empty())
    {
        return 0.0f;
    }
    std::vector<float> sorted(values.begin(), values.end());
    std::ranges::sort(sorted);
    auto mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0)
    {
        return (sorted[mid - 1] + sorted[mid]) / 2.0f;
    }
    return sorted[mid];
}

} // namespace Dss::Math
