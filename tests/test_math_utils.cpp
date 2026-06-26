#include <vector>

#include <gtest/gtest.h>

#include "dss/tracking/math_utils.h"

namespace {

constexpr auto kTolerance = 1.0e-9;

}  // namespace

TEST(MathUtils, LinearFitMatchesLegacyMpolyfitStatistics) {
    const std::vector<double> x{0.0, 1.0, 2.0, 3.0};
    const std::vector<double> y{1.0, 3.0, 5.0, 7.0};

    const auto result = Dss::Math::linearFit(x, y);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->coefficients.size(), 2U);
    EXPECT_NEAR(result->coefficients[0], 1.0, kTolerance);
    EXPECT_NEAR(result->coefficients[1], 2.0, kTolerance);
    ASSERT_EQ(result->fittedY.size(), y.size());
    EXPECT_NEAR(result->fittedY[0], 1.0, kTolerance);
    EXPECT_NEAR(result->fittedY[1], 3.0, kTolerance);
    EXPECT_NEAR(result->fittedY[2], 5.0, kTolerance);
    EXPECT_NEAR(result->fittedY[3], 7.0, kTolerance);
    EXPECT_NEAR(result->ssr, 20.0, kTolerance);
    EXPECT_NEAR(result->sse, 0.0, kTolerance);
    EXPECT_NEAR(result->rmse, 0.0, kTolerance);
    EXPECT_NEAR(result->rSquared, 1.0, kTolerance);
}

TEST(MathUtils, PolynomialFitMatchesLegacyCoefficientOrder) {
    const std::vector<double> x{-2.0, -1.0, 0.0, 1.0, 2.0};
    const std::vector<double> y{9.0, 5.5, 3.0, 1.5, 1.0};

    const auto result = Dss::Math::polynomialFit(x, y, 2);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->coefficients.size(), 3U);
    EXPECT_NEAR(result->coefficients[0], 3.0, kTolerance);
    EXPECT_NEAR(result->coefficients[1], -2.0, kTolerance);
    EXPECT_NEAR(result->coefficients[2], 0.5, kTolerance);
    EXPECT_NEAR(Dss::Math::polyval(result->coefficients, 4.0), 3.0, kTolerance);
    EXPECT_NEAR(result->sse, 0.0, kTolerance);
    EXPECT_NEAR(result->rmse, 0.0, kTolerance);
    EXPECT_NEAR(result->rSquared, 1.0, kTolerance);
}

TEST(MathUtils, PolynomialFitCanSkipSavingFittedValuesWhileKeepingErrors) {
    const std::vector<double> x{0.0, 1.0, 2.0, 3.0};
    const std::vector<double> y{1.0, 2.0, 4.0, 8.0};

    const auto result = Dss::Math::polynomialFit(x, y, 1, false);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->fittedY.empty());
    EXPECT_GT(result->ssr, 0.0);
    EXPECT_GT(result->sse, 0.0);
    EXPECT_GT(result->rmse, 0.0);
    EXPECT_GT(result->rSquared, 0.0);
    EXPECT_LT(result->rSquared, 1.0);
}

TEST(MathUtils, PolynomialFitRejectsInvalidInputsInsteadOfDividingByZero) {
    const std::vector<double> x{1.0, 1.0, 1.0};
    const std::vector<double> y{1.0, 2.0, 3.0};
    const std::vector<double> empty;
    const std::vector<double> shortY{1.0, 2.0};

    EXPECT_FALSE(Dss::Math::polynomialFit(empty, empty, 1).has_value());
    EXPECT_FALSE(Dss::Math::polynomialFit(x, shortY, 1).has_value());
    EXPECT_FALSE(Dss::Math::polynomialFit(x, y, -1).has_value());
    EXPECT_FALSE(Dss::Math::polynomialFit(x, y, 3).has_value());
    EXPECT_FALSE(Dss::Math::polynomialFit(x, y, 1).has_value());
    EXPECT_FALSE(Dss::Math::linearFit(x, y).has_value());
    EXPECT_TRUE(Dss::Math::polyfit(x, shortY, 1).empty());
}

TEST(MathUtils, ComputesLegacySamplePeriodError) {
    const std::vector<double> samples{0.0, 1.0, 2.02, 3.01};
    const std::vector<double> empty;

    const auto error = Dss::Math::samplePeriodError(samples, 1.0);

    ASSERT_TRUE(error.has_value());
    EXPECT_NEAR(*error, 0.02, kTolerance);
    EXPECT_FALSE(Dss::Math::samplePeriodError(empty, 1.0).has_value());
    EXPECT_FALSE(Dss::Math::samplePeriodError(samples, 0.0).has_value());
}

TEST(MathUtils, FindsLegacyMinimumSamplePeriod) {
    const std::vector<double> regularSamples{0.0, 1.0, 2.0, 3.0};
    const std::vector<double> singleSample{0.0};
    const std::vector<double> nonIncreasingSamples{0.0, 1.0, 0.5};

    const auto estimate = Dss::Math::minimumSamplePeriod(regularSamples, 0.2);

    ASSERT_TRUE(estimate.has_value());
    EXPECT_NEAR(estimate->period, 1.0, kTolerance);
    EXPECT_NEAR(estimate->maxError, 0.0, kTolerance);
    EXPECT_FALSE(Dss::Math::minimumSamplePeriod(singleSample, 0.2).has_value());
    EXPECT_FALSE(Dss::Math::minimumSamplePeriod(nonIncreasingSamples, 0.2).has_value());
}

TEST(MathUtils, RemovesPolynomialTrendLikeLegacyGetPeriod) {
    const std::vector<double> x{0.0, 1.0, 2.0, 3.0};
    const std::vector<double> y{2.0, 6.0, 8.0, 8.0};

    const auto detrended = Dss::Math::removePolynomialTrend(x, y, 1);

    ASSERT_TRUE(detrended.has_value());
    ASSERT_EQ(detrended->coefficients.size(), 2U);
    EXPECT_NEAR(detrended->coefficients[0], 3.0, kTolerance);
    EXPECT_NEAR(detrended->coefficients[1], 2.0, kTolerance);
    EXPECT_EQ(detrended->fittedY.size(), y.size());
    EXPECT_EQ(detrended->residuals.size(), y.size());
    EXPECT_NEAR(detrended->fittedY[0], 3.0, kTolerance);
    EXPECT_NEAR(detrended->fittedY[3], 9.0, kTolerance);
    EXPECT_NEAR(detrended->residuals[0], -1.0, kTolerance);
    EXPECT_NEAR(detrended->residuals[1], 1.0, kTolerance);
    EXPECT_NEAR(detrended->residuals[2], 1.0, kTolerance);
    EXPECT_NEAR(detrended->residuals[3], -1.0, kTolerance);
}

TEST(MathUtils, AppliesLegacyNearestSampleInterpolation) {
    const std::vector<double> x{0.0, 2.0, 4.0};
    const std::vector<double> y{10.0, 20.0, 40.0};
    const std::vector<double> targetX{0.0, 1.0, 2.0, 3.0, 4.0};

    const auto interpolated = Dss::Math::legacyNearestSampleInterpolate(x, y, targetX);

    ASSERT_TRUE(interpolated.has_value());
    const std::vector<double> expected{10.0, 0.0, 20.0, 0.0, 40.0};
    EXPECT_EQ(*interpolated, expected);

    const std::vector<double> singleX{7.0};
    const std::vector<double> singleY{42.0};
    const auto single = Dss::Math::legacyNearestSampleInterpolate(singleX, singleY, targetX);
    ASSERT_TRUE(single.has_value());
    EXPECT_EQ(*single, std::vector<double>(targetX.size(), 42.0));
    const std::vector<double> shortY{10.0};
    EXPECT_FALSE(Dss::Math::legacyNearestSampleInterpolate(x, shortY, targetX).has_value());
}

TEST(MathUtils, ComputesLegacyFftSizeLikeMfftProcess) {
    EXPECT_FALSE(Dss::Math::legacyFftSize(0).has_value());
    EXPECT_EQ(Dss::Math::legacyFftSize(1), 2U);
    EXPECT_EQ(Dss::Math::legacyFftSize(2), 2U);
    EXPECT_EQ(Dss::Math::legacyFftSize(3), 4U);
    EXPECT_EQ(Dss::Math::legacyFftSize(4), 4U);
    EXPECT_EQ(Dss::Math::legacyFftSize(5), 8U);
}

TEST(MathUtils, BuildsLegacyFftSpectrumWithAmplitudePhaseAndBaseFrequency) {
    const std::vector<double> signal{1.0, 0.0, -1.0, 0.0};

    const auto spectrum = Dss::Math::legacyFftSpectrum(signal, 0.5);

    ASSERT_TRUE(spectrum.has_value());
    EXPECT_EQ(spectrum->fftSize, 4U);
    EXPECT_NEAR(spectrum->baseFrequency, 1.0 / 1.5, kTolerance);
    ASSERT_EQ(spectrum->real.size(), 4U);
    ASSERT_EQ(spectrum->imag.size(), 4U);
    ASSERT_EQ(spectrum->amplitude.size(), 4U);
    ASSERT_EQ(spectrum->phaseDegrees.size(), 4U);
    EXPECT_NEAR(spectrum->real[1], 2.0, kTolerance);
    EXPECT_NEAR(spectrum->imag[1], 0.0, kTolerance);
    EXPECT_NEAR(spectrum->amplitude[0], 0.0, kTolerance);
    EXPECT_NEAR(spectrum->amplitude[1], 1.0, kTolerance);
    EXPECT_NEAR(spectrum->phaseDegrees[1], 0.0, kTolerance);
    EXPECT_NEAR(spectrum->peakAmplitude, 1.0, kTolerance);
}

TEST(MathUtils, PadsLegacyFftSpectrumInputToNextPowerOfTwo) {
    const std::vector<double> signal{1.0, 0.0, -1.0};

    const auto spectrum = Dss::Math::legacyFftSpectrum(signal, 1.0);

    ASSERT_TRUE(spectrum.has_value());
    EXPECT_EQ(spectrum->fftSize, 4U);
    EXPECT_NEAR(spectrum->baseFrequency, 1.0 / 3.0, kTolerance);
    EXPECT_NEAR(spectrum->real[1], 2.0, kTolerance);
    EXPECT_NEAR(spectrum->amplitude[1], 1.0, kTolerance);
}

TEST(MathUtils, RejectsInvalidLegacyFftSpectrumInputs) {
    const std::vector<double> signal{1.0, 0.0, -1.0};
    const std::vector<double> imag{0.0};
    const std::vector<double> empty;

    EXPECT_FALSE(Dss::Math::legacyFftSpectrum(empty, 1.0).has_value());
    EXPECT_FALSE(Dss::Math::legacyFftSpectrum(signal, 0.0).has_value());
    EXPECT_FALSE(Dss::Math::legacyFftSpectrum(signal, imag, 1.0).has_value());
}
