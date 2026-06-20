#include <array>
#include <filesystem>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "dss/tracking/geo_tracker.h"
#include "dss/tracking/leo_tracker.h"
#include "dss/tracking/manual_tracker.h"
#include "dss/tracking/sc_tracker.h"
#include "tracking_fixture_loader.h"

namespace {

using Dss::Tests::TrackingFixture;
using Dss::Tests::TrackingFixtureMode;

[[nodiscard]] auto makeTracker(const TrackingFixture& fixture)
    -> std::unique_ptr<Dss::Tracking::ITrackingStrategy> {
    switch (fixture.mode) {
        case TrackingFixtureMode::Geo:
            return std::make_unique<Dss::Tracking::GeoTracker>(fixture.settings);
        case TrackingFixtureMode::Leo:
            return std::make_unique<Dss::Tracking::LeoTracker>(fixture.settings);
        case TrackingFixtureMode::Sc:
            return std::make_unique<Dss::Tracking::ScTracker>(fixture.settings);
        case TrackingFixtureMode::Manual:
            return std::make_unique<Dss::Tracking::ManualTracker>(fixture.settings);
    }
    return {};
}

void verifyFixture(const TrackingFixture& fixture) {
    auto tracker = makeTracker(fixture);
    ASSERT_NE(tracker, nullptr);

    for (const auto& frame : fixture.frames) {
        if (frame.reset) {
            tracker->reset();
        }
        if (frame.manualTarget.has_value()) {
            auto* manual = dynamic_cast<Dss::Tracking::ManualTracker*>(tracker.get());
            ASSERT_NE(manual, nullptr);
            manual->setManualTarget(*frame.manualTarget);
        }

        const auto targets = tracker->track(frame.measurements);
        if (targets.size() != frame.expected.targetCount) {
            ADD_FAILURE() << fixture.name << " frame " << frame.measurements.frameSeq
                          << ": expected " << frame.expected.targetCount << " targets, got "
                          << targets.size();
            continue;
        }

        for (const auto& expected : frame.expected.targets) {
            const auto found = std::ranges::find_if(
                targets, [&](const auto& target) { return target.targetId == expected.targetId; });
            ASSERT_NE(found, targets.end()) << expected.targetId;
            ASSERT_FALSE(found->frameInfos.empty());
            EXPECT_EQ(found->living, expected.living);
            EXPECT_EQ(found->frameInfos.size(), expected.historySize);
            EXPECT_EQ(found->frameInfos.back().valid, expected.latestValid);
            EXPECT_FLOAT_EQ(found->frameInfos.back().posTwdw.x, expected.posTwdw.x);
            EXPECT_FLOAT_EQ(found->frameInfos.back().posTwdw.y, expected.posTwdw.y);
            EXPECT_FLOAT_EQ(found->frameInfos.back().measuredBlob.dn, expected.dn);
            EXPECT_FLOAT_EQ(found->frameInfos.back().magnitude, expected.magnitude);
        }
    }
}

}  // namespace

TEST(TrackingFixtureLoader, RejectsMalformedJson) {
    const auto result = Dss::Tests::parseTrackingFixture("{not-json}");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("JSON"), std::string::npos);
}

TEST(TrackingFixtureLoader, RejectsMissingRequiredFields) {
    const auto result = Dss::Tests::parseTrackingFixture(R"({"schema_version":1,"mode":"leo"})");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("name"), std::string::npos);
}

TEST(TrackingFixtureLoader, RejectsNumericBoundaryViolations) {
    const auto result = Dss::Tests::parseTrackingFixture(R"({
        "schema_version": 1,
        "name": "bad-frame",
        "mode": "leo",
        "frames": [{"frame_seq": 0, "expected": {"target_count": 0}}]
    })");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("frame_seq"), std::string::npos);
}

TEST(TrackingLegacyScenarios, MatchesReviewableGoldenFixtures) {
    constexpr std::array fixtureNames{
        "geo_hit_miss_rediscovery.json",
        "leo_hit_miss_rediscovery.json",
        "sc_hit_miss_rediscovery.json",
        "manual_hit_miss_rediscovery.json",
    };

    for (const auto* fixtureName : fixtureNames) {
        const auto fixturePath = std::filesystem::path{DSS_TRACKING_FIXTURE_DIR} / fixtureName;
        const auto fixture = Dss::Tests::loadTrackingFixture(fixturePath);
        if (!fixture.has_value()) {
            ADD_FAILURE() << fixturePath << ": " << fixture.error();
            continue;
        }
        verifyFixture(*fixture);
    }
}
