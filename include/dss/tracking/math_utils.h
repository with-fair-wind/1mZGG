#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace Dss::Math {

struct PolynomialFitResult {
    std::vector<double> coefficients;
    std::vector<double> fittedY;
    double ssr = 0.0;
    double sse = 0.0;
    double rmse = 0.0;
    double rSquared = 0.0;
};

struct SamplePeriodEstimate {
    double period = 0.0;
    double maxError = 0.0;
};

struct DetrendedSeries {
    std::vector<double> coefficients;
    std::vector<double> fittedY;
    std::vector<double> residuals;
};

auto polyfit(std::span<const double> x, std::span<const double> y, int order)
    -> std::vector<double>;

auto polynomialFit(std::span<const double> x, std::span<const double> y, int order,
                   bool saveFittedY = true) -> std::optional<PolynomialFitResult>;

auto linearFit(std::span<const double> x, std::span<const double> y, bool saveFittedY = true)
    -> std::optional<PolynomialFitResult>;

auto samplePeriodError(std::span<const double> samples, double samplePeriod)
    -> std::optional<double>;

auto minimumSamplePeriod(std::span<const double> samples, double errorLimitRate = 0.2)
    -> std::optional<SamplePeriodEstimate>;

auto removePolynomialTrend(std::span<const double> x, std::span<const double> y, int order)
    -> std::optional<DetrendedSeries>;

auto legacyNearestSampleInterpolate(std::span<const double> x, std::span<const double> y,
                                    std::span<const double> targetX)
    -> std::optional<std::vector<double>>;

auto polyval(std::span<const double> coeffs, double x) -> double;

void fft(std::span<const double> input, std::span<double> outputReal, std::span<double> outputImag);

void ifft(std::span<const double> inputReal, std::span<const double> inputImag,
          std::span<double> output);

auto estimatePeriod(std::span<const double> signal, double sampleRate) -> double;

auto median(std::span<const float> values) -> float;

}  // namespace Dss::Math
