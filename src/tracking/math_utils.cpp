#include "dss/tracking/math_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <vector>

namespace Dss::Math {

namespace {

constexpr auto kSingularTolerance = 1.0e-12;

auto validFitInput(std::span<const double> x, std::span<const double> y, int order) -> bool {
    return order >= 0 && !x.empty() && x.size() == y.size() &&
           x.size() >= static_cast<std::size_t>(order + 1);
}

auto solveLinearSystem(std::vector<double> matrix, std::vector<double> rhs, int size)
    -> std::optional<std::vector<double>> {
    for (int column = 0; column < size; ++column) {
        auto pivotRow = column;
        auto pivotAbs = std::abs(matrix[column * size + column]);
        for (int row = column + 1; row < size; ++row) {
            const auto candidate = std::abs(matrix[row * size + column]);
            if (candidate > pivotAbs) {
                pivotAbs = candidate;
                pivotRow = row;
            }
        }
        if (pivotAbs <= kSingularTolerance) {
            return std::nullopt;
        }

        if (pivotRow != column) {
            for (int index = 0; index < size; ++index) {
                std::swap(matrix[column * size + index], matrix[pivotRow * size + index]);
            }
            std::swap(rhs[column], rhs[pivotRow]);
        }

        for (int row = column + 1; row < size; ++row) {
            const auto factor = matrix[row * size + column] / matrix[column * size + column];
            matrix[row * size + column] = 0.0;
            for (int index = column + 1; index < size; ++index) {
                matrix[row * size + index] -= factor * matrix[column * size + index];
            }
            rhs[row] -= factor * rhs[column];
        }
    }

    std::vector<double> solution(size, 0.0);
    for (int row = size - 1; row >= 0; --row) {
        auto value = rhs[row];
        for (int column = row + 1; column < size; ++column) {
            value -= matrix[row * size + column] * solution[column];
        }
        const auto diagonal = matrix[row * size + row];
        if (std::abs(diagonal) <= kSingularTolerance) {
            return std::nullopt;
        }
        solution[row] = value / diagonal;
    }
    return solution;
}

auto fitCoefficients(std::span<const double> x, std::span<const double> y, int order)
    -> std::optional<std::vector<double>> {
    if (!validFitInput(x, y, order)) {
        return std::nullopt;
    }

    const auto matrixSize = order + 1;
    std::vector<double> matrix(matrixSize * matrixSize, 0.0);
    std::vector<double> rhs(matrixSize, 0.0);

    for (std::size_t sample = 0; sample < x.size(); ++sample) {
        std::vector<double> powers(static_cast<std::size_t>(order * 2 + 1), 1.0);
        for (std::size_t power = 1; power < powers.size(); ++power) {
            powers[power] = powers[power - 1] * x[sample];
        }

        for (int row = 0; row < matrixSize; ++row) {
            rhs[row] += y[sample] * powers[static_cast<std::size_t>(row)];
            for (int column = 0; column < matrixSize; ++column) {
                matrix[row * matrixSize + column] += powers[static_cast<std::size_t>(row + column)];
            }
        }
    }

    return solveLinearSystem(std::move(matrix), std::move(rhs), matrixSize);
}

auto buildFitResult(std::span<const double> x, std::span<const double> y,
                    std::vector<double> coefficients, bool saveFittedY) -> PolynomialFitResult {
    PolynomialFitResult result{};
    result.coefficients = std::move(coefficients);
    if (saveFittedY) {
        result.fittedY.reserve(y.size());
    }

    const auto meanY = std::accumulate(y.begin(), y.end(), 0.0) / static_cast<double>(y.size());
    for (std::size_t index = 0; index < y.size(); ++index) {
        const auto fitted = polyval(result.coefficients, x[index]);
        result.ssr += (fitted - meanY) * (fitted - meanY);
        result.sse += (fitted - y[index]) * (fitted - y[index]);
        if (saveFittedY) {
            result.fittedY.push_back(fitted);
        }
    }

    result.rmse = std::sqrt(result.sse / static_cast<double>(y.size()));
    const auto total = result.ssr + result.sse;
    if (total > std::numeric_limits<double>::epsilon()) {
        result.rSquared = 1.0 - result.sse / total;
    } else {
        result.rSquared = (result.sse <= kSingularTolerance) ? 1.0 : 0.0;
    }
    return result;
}

auto findFirstGreater(std::span<const double> data, double threshold, std::size_t startIndex)
    -> std::optional<std::size_t> {
    if (data.empty()) {
        return std::nullopt;
    }
    auto start = std::min(startIndex, data.size() - 1);
    for (auto index = start; index < data.size(); ++index) {
        if (data[index] > threshold) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace

auto polyfit(std::span<const double> x, std::span<const double> y, int order)
    -> std::vector<double> {
    const auto result = polynomialFit(x, y, order, false);
    if (!result.has_value()) {
        return {};
    }
    return result->coefficients;
}

auto polynomialFit(std::span<const double> x, std::span<const double> y, int order,
                   bool saveFittedY) -> std::optional<PolynomialFitResult> {
    auto coefficients = fitCoefficients(x, y, order);
    if (!coefficients.has_value()) {
        return std::nullopt;
    }
    return buildFitResult(x, y, std::move(*coefficients), saveFittedY);
}

auto linearFit(std::span<const double> x, std::span<const double> y, bool saveFittedY)
    -> std::optional<PolynomialFitResult> {
    return polynomialFit(x, y, 1, saveFittedY);
}

auto samplePeriodError(std::span<const double> samples, double samplePeriod)
    -> std::optional<double> {
    if (samples.empty() || samplePeriod <= 0.0) {
        return std::nullopt;
    }

    const auto errorLimit = std::abs(samplePeriod / 2.0);
    std::vector<double> errors;
    errors.reserve(samples.size());
    auto meanError = 0.0;
    for (const auto sample : samples) {
        auto error = std::fmod(std::abs(sample), samplePeriod);
        if (error > errorLimit) {
            error -= errorLimit;
        } else if (error < -errorLimit) {
            error += errorLimit;
        }
        errors.push_back(error);
        meanError += error;
    }
    meanError /= static_cast<double>(samples.size());

    auto maxError = *std::ranges::max_element(errors);
    if (samples.size() > 2 && maxError > 3.0 * meanError) {
        for (auto& error : errors) {
            if (error > errorLimit) {
                error = 0.0;
            }
        }
        maxError = *std::ranges::max_element(errors);
    }
    return maxError;
}

auto minimumSamplePeriod(std::span<const double> samples, double errorLimitRate)
    -> std::optional<SamplePeriodEstimate> {
    if (samples.size() < 2 || errorLimitRate < 0.0) {
        return std::nullopt;
    }

    std::vector<double> deltas;
    deltas.reserve(samples.size() - 1);
    for (std::size_t index = 0; index + 1 < samples.size(); ++index) {
        deltas.push_back(samples[index + 1] - samples[index]);
    }
    const auto minDelta = *std::ranges::min_element(deltas);
    if (minDelta <= 0.0) {
        return std::nullopt;
    }

    for (auto coefficient = 0; coefficient < 5; ++coefficient) {
        const auto period = minDelta * std::pow(2.0, -coefficient);
        const auto maxError = samplePeriodError(samples, period);
        if (!maxError.has_value()) {
            return std::nullopt;
        }
        if (*maxError < errorLimitRate * minDelta) {
            return SamplePeriodEstimate{.period = period, .maxError = *maxError};
        }
    }
    return std::nullopt;
}

auto removePolynomialTrend(std::span<const double> x, std::span<const double> y, int order)
    -> std::optional<DetrendedSeries> {
    const auto fit = polynomialFit(x, y, order, true);
    if (!fit.has_value()) {
        return std::nullopt;
    }

    DetrendedSeries detrended{};
    detrended.coefficients = fit->coefficients;
    detrended.fittedY = fit->fittedY;
    detrended.residuals.reserve(y.size());
    for (std::size_t index = 0; index < y.size(); ++index) {
        detrended.residuals.push_back(y[index] - detrended.fittedY[index]);
    }
    return detrended;
}

auto legacyNearestSampleInterpolate(std::span<const double> x, std::span<const double> y,
                                    std::span<const double> targetX)
    -> std::optional<std::vector<double>> {
    if (x.empty() || x.size() != y.size() || targetX.empty()) {
        return std::nullopt;
    }
    if (x.size() == 1) {
        return std::vector<double>(targetX.size(), y.front());
    }

    std::vector<double> interpolated(targetX.size(), 0.0);
    std::size_t startIndex = 0;
    for (std::size_t index = 0; index < targetX.size(); ++index) {
        double deltaX = 0.0;
        if (index + 1 < targetX.size()) {
            deltaX = targetX[index + 1] - targetX[index];
        } else if (targetX.size() > 1) {
            deltaX = targetX[index] - targetX[index - 1];
        }
        deltaX = std::abs(deltaX);

        const auto elementIndex = findFirstGreater(x, targetX[index], startIndex);
        if (!elementIndex.has_value()) {
            interpolated[index] = y.back();
            startIndex = 0;
        } else if (*elementIndex == 0) {
            interpolated[index] = y.front();
            startIndex = 0;
        } else {
            if (std::abs(x[*elementIndex] - targetX[index]) < deltaX * 0.5) {
                interpolated[index] = y[*elementIndex];
            } else if (std::abs(x[*elementIndex - 1] - targetX[index]) < deltaX * 0.5) {
                interpolated[index] = y[*elementIndex - 1];
            }
            startIndex = *elementIndex;
        }
    }
    return interpolated;
}

auto polyval(std::span<const double> coeffs, double x) -> double {
    double result = 0.0;
    double xn = 1.0;
    for (double c : coeffs) {
        result += c * xn;
        xn *= x;
    }
    return result;
}

void fft(std::span<const double> input, std::span<double> outputReal,
         std::span<double> outputImag) {
    const auto n = input.size();
    for (size_t k = 0; k < n; ++k) {
        double sumReal = 0.0;
        double sumImag = 0.0;
        for (size_t t = 0; t < n; ++t) {
            double angle =
                2.0 * std::numbers::pi * static_cast<double>(k * t) / static_cast<double>(n);
            sumReal += input[t] * std::cos(angle);
            sumImag -= input[t] * std::sin(angle);
        }
        outputReal[k] = sumReal;
        outputImag[k] = sumImag;
    }
}

void ifft(std::span<const double> inputReal, std::span<const double> inputImag,
          std::span<double> output) {
    const auto n = inputReal.size();
    for (size_t t = 0; t < n; ++t) {
        double sum = 0.0;
        for (size_t k = 0; k < n; ++k) {
            double angle =
                2.0 * std::numbers::pi * static_cast<double>(k * t) / static_cast<double>(n);
            sum += inputReal[k] * std::cos(angle) - inputImag[k] * std::sin(angle);
        }
        output[t] = sum / static_cast<double>(n);
    }
}

auto estimatePeriod(std::span<const double> signal, double sampleRate) -> double {
    const auto n = signal.size();
    std::vector<double> real(n);
    std::vector<double> imag(n);
    fft(signal, real, imag);

    double maxMagnitude = 0.0;
    size_t maxIndex = 1;
    for (size_t k = 1; k < n / 2; ++k) {
        double mag = std::sqrt(real[k] * real[k] + imag[k] * imag[k]);
        if (mag > maxMagnitude) {
            maxMagnitude = mag;
            maxIndex = k;
        }
    }

    double freq = static_cast<double>(maxIndex) * sampleRate / static_cast<double>(n);
    return (freq > 0.0) ? 1.0 / freq : 0.0;
}

auto median(std::span<const float> values) -> float {
    if (values.empty()) {
        return 0.0f;
    }
    std::vector<float> sorted(values.begin(), values.end());
    std::ranges::sort(sorted);
    auto mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
        return (sorted[mid - 1] + sorted[mid]) / 2.0f;
    }
    return sorted[mid];
}

}  // namespace Dss::Math
