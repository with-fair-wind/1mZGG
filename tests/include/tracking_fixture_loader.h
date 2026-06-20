#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Tests {

enum class TrackingFixtureMode { Geo, Leo, Sc, Manual };

struct ExpectedTrackingTarget {
    std::string targetId;
    bool living = false;
    std::size_t historySize = 0;
    bool latestValid = false;
    Dss::Core::Vec2f posTwdw{};
    float dn = 0.0F;
    float magnitude = 0.0F;
};

struct ExpectedTrackingFrame {
    std::size_t targetCount = 0;
    std::vector<ExpectedTrackingTarget> targets;
};

struct TrackingFixtureFrame {
    Dss::Core::FrameMeasurements measurements;
    std::optional<Dss::Core::MeasuredBlob> manualTarget;
    bool reset = false;
    ExpectedTrackingFrame expected;
};

struct TrackingFixture {
    std::string name;
    TrackingFixtureMode mode = TrackingFixtureMode::Geo;
    Dss::Core::TrackingSettings settings;
    std::vector<TrackingFixtureFrame> frames;
};

[[nodiscard]] auto parseTrackingFixture(std::string_view jsonText)
    -> std::expected<TrackingFixture, std::string>;
[[nodiscard]] auto loadTrackingFixture(const std::filesystem::path& path)
    -> std::expected<TrackingFixture, std::string>;

}  // namespace Dss::Tests
