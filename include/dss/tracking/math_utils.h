#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace Dss::Math
{

auto polyfit(std::span<const double> x, std::span<const double> y, int order) -> std::vector<double>;

auto polyval(std::span<const double> coeffs, double x) -> double;

void fft(std::span<const double> input, std::span<double> outputReal, std::span<double> outputImag);

void ifft(std::span<const double> inputReal,
          std::span<const double> inputImag,
          std::span<double> output);

auto estimatePeriod(std::span<const double> signal, double sampleRate) -> double;

auto median(std::span<const float> values) -> float;

} // namespace Dss::Math
