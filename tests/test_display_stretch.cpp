#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "dss/processing/display_stretch.h"

TEST(DisplayStretch, ManualWindowUsesLegacyClampAndLinearMapping) {
    const std::vector<std::uint16_t> raw{500, 1000, 3000, 5000, 6000};
    const auto display = Dss::Processing::stretchDisplayImage(
        raw, Dss::Processing::DisplayStretchWindow{.low = 1000, .high = 5000});

    EXPECT_EQ(display, (std::vector<std::uint8_t>{0, 0, 127, 255, 255}));
}

TEST(DisplayStretch, AutoWindowUsesLegacyAveragePlusMinusThreeSigma) {
    Dss::Core::ImageStats stats{};
    stats.avg = 1466.4;
    stats.stdDev = 130.9;

    const auto window = Dss::Processing::resolveDisplayStretchWindow(
        stats, Dss::Processing::DisplayStretchSettings{});

    EXPECT_EQ(window.low, 1073U);
    EXPECT_EQ(window.high, 1859U);
}

TEST(DisplayStretch, BuildDisplayImageReturnsStatsWindowAndPixels) {
    const std::vector<std::uint16_t> raw{1000, 1000, 1500, 2000};
    Dss::Processing::DisplayStretchSettings settings{};
    settings.mode = Dss::Processing::DisplayStretchMode::Manual;
    settings.low = 1000;
    settings.high = 2000;

    const auto result = Dss::Processing::buildDisplayImage(raw, settings);

    EXPECT_DOUBLE_EQ(result.stats.minVal, 1000.0);
    EXPECT_DOUBLE_EQ(result.stats.maxVal, 2000.0);
    EXPECT_EQ(result.window.low, 1000U);
    EXPECT_EQ(result.window.high, 2000U);
    EXPECT_EQ(result.displayImage, (std::vector<std::uint8_t>{0, 0, 127, 255}));
}
