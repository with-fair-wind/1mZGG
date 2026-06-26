#include "dss/processing/display_stretch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Dss::Processing {
namespace {

using DisplayLookupTable = std::array<
    std::uint8_t,
    static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U>;

[[nodiscard]] auto clampToWindowLimit(double value, std::uint16_t minimum,
                                      std::uint16_t maximum) -> std::uint16_t {
    const auto clamped = std::clamp(value, static_cast<double>(minimum),
                                   static_cast<double>(maximum));
    return static_cast<std::uint16_t>(clamped);
}

[[nodiscard]] auto normalizeWindow(std::uint16_t low, std::uint16_t high,
                                   const DisplayStretchSettings& settings)
    -> DisplayStretchWindow {
    const auto boundedLow = std::clamp(low, settings.signalMin, settings.signalMax);
    auto boundedHigh = std::clamp(high, settings.signalMin, settings.signalMax);
    if (boundedHigh > boundedLow) {
        return DisplayStretchWindow{.low = boundedLow, .high = boundedHigh};
    }

    if (boundedLow < settings.signalMax) {
        boundedHigh = static_cast<std::uint16_t>(boundedLow + 1U);
        return DisplayStretchWindow{.low = boundedLow, .high = boundedHigh};
    }

    const auto adjustedLow =
        settings.signalMax > settings.signalMin
            ? static_cast<std::uint16_t>(settings.signalMax - 1U)
            : settings.signalMin;
    return DisplayStretchWindow{.low = adjustedLow, .high = settings.signalMax};
}

[[nodiscard]] auto fallbackWindowFromMinMax(const Dss::Core::ImageStats& stats,
                                            const DisplayStretchSettings& settings)
    -> DisplayStretchWindow {
    const auto low = clampToWindowLimit(stats.minVal, settings.signalMin, settings.signalMax);
    const auto high = clampToWindowLimit(stats.maxVal, settings.signalMin, settings.signalMax);
    return normalizeWindow(low, high, settings);
}

[[nodiscard]] auto buildDisplayLookupTable(DisplayStretchWindow window) -> DisplayLookupTable {
    DisplayLookupTable table{};
    if (window.high <= window.low) {
        return table;
    }

    const auto low = static_cast<std::uint32_t>(window.low);
    const auto high = static_cast<std::uint32_t>(window.high);
    const auto range = high - low;
    for (auto value = low + 1U; value < high; ++value) {
        table[value] = static_cast<std::uint8_t>(((value - low) * 255U) / range);
    }
    std::fill(table.begin() + static_cast<std::ptrdiff_t>(high), table.end(), std::uint8_t{255});
    return table;
}

}  // namespace

auto computeImageStats(std::span<const std::uint16_t> raw) -> Dss::Core::ImageStats {
    Dss::Core::ImageStats stats{};
    if (raw.empty()) {
        return stats;
    }

    auto minValue = std::numeric_limits<std::uint16_t>::max();
    auto maxValue = std::uint16_t{0};
    std::uint64_t sum = 0;
    std::uint64_t sumSquares = 0;

    for (const auto value : raw) {
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
        const auto wideValue = static_cast<std::uint64_t>(value);
        sum += wideValue;
        sumSquares += wideValue * wideValue;
    }

    const auto count = static_cast<long double>(raw.size());
    const auto mean = static_cast<long double>(sum) / count;
    const auto meanSquares = static_cast<long double>(sumSquares) / count;
    const auto variance = std::max(0.0L, meanSquares - mean * mean);

    stats.minVal = static_cast<double>(minValue);
    stats.maxVal = static_cast<double>(maxValue);
    stats.avg = static_cast<double>(mean);
    stats.stdDev = static_cast<double>(std::sqrt(variance));
    return stats;
}

auto resolveDisplayStretchWindow(const Dss::Core::ImageStats& stats,
                                 const DisplayStretchSettings& settings)
    -> DisplayStretchWindow {
    if (settings.mode == DisplayStretchMode::Manual) {
        return normalizeWindow(settings.low, settings.high, settings);
    }

    const auto lowValue = stats.avg - settings.sigma * stats.stdDev;
    const auto highValue = stats.avg + settings.sigma * stats.stdDev;
    const auto low = clampToWindowLimit(lowValue, settings.signalMin, settings.signalMax);
    const auto high = clampToWindowLimit(highValue, settings.signalMin, settings.signalMax);
    if (high > low) {
        return DisplayStretchWindow{.low = low, .high = high};
    }

    return fallbackWindowFromMinMax(stats, settings);
}

auto stretchDisplayImage(std::span<const std::uint16_t> raw, DisplayStretchWindow window)
    -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> display(raw.size());
    if (raw.empty() || window.high <= window.low) {
        return display;
    }

    const auto lookup = buildDisplayLookupTable(window);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        display[index] = lookup[raw[index]];
    }
    return display;
}

auto buildDisplayImage(std::span<const std::uint16_t> raw,
                       const DisplayStretchSettings& settings) -> DisplayStretchResult {
    DisplayStretchResult result{};
    result.stats = computeImageStats(raw);
    result.window = resolveDisplayStretchWindow(result.stats, settings);
    result.displayImage = stretchDisplayImage(raw, result.window);
    return result;
}

}  // namespace Dss::Processing
