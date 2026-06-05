#include <gtest/gtest.h>

#include "dss/tracking/track_manager.h"

namespace {

auto makeSettings() -> Dss::Core::TrackingSettings {
    Dss::Core::TrackingSettings settings{};
    settings.opticParams.imageWidth = 1000;
    settings.opticParams.imageHeight = 1000;
    settings.opticParams.fovCenterX = 500.0F;
    settings.opticParams.fovCenterY = 500.0F;
    settings.opticParams.pixelScale = 0.01F;
    settings.searchRadius = 20.0F;
    return settings;
}

auto makeMeasurements() -> Dss::Core::FrameMeasurements {
    Dss::Core::FrameMeasurements measurements{};
    measurements.frameSeq = 1;
    measurements.frameFreq = 1.0F;
    return measurements;
}

}  // namespace

TEST(TrackManager, SetModeCreatesConcreteTrackingStrategies) {
    Dss::Tracking::TrackManager manager;
    const auto settings = makeSettings();

    manager.setMode(Dss::Core::TrackMode::Geo, settings);
    EXPECT_EQ(manager.currentMode(), Dss::Core::TrackMode::Geo);

    manager.setMode(Dss::Core::TrackMode::Manual, settings);
    EXPECT_EQ(manager.currentMode(), Dss::Core::TrackMode::Manual);

    manager.setMode(Dss::Core::TrackMode::Leo, settings);
    EXPECT_EQ(manager.currentMode(), Dss::Core::TrackMode::Leo);

    manager.setMode(Dss::Core::TrackMode::SpaceCatalog, settings);
    EXPECT_EQ(manager.currentMode(), Dss::Core::TrackMode::SpaceCatalog);
}

TEST(TrackManager, SetModeInitClearsActiveStrategy) {
    Dss::Tracking::TrackManager manager;
    const auto settings = makeSettings();

    manager.setMode(Dss::Core::TrackMode::Geo, settings);
    ASSERT_EQ(manager.currentMode(), Dss::Core::TrackMode::Geo);

    manager.setMode(Dss::Core::TrackMode::Init, settings);

    EXPECT_EQ(manager.currentMode(), Dss::Core::TrackMode::Init);
    EXPECT_TRUE(manager.track(makeMeasurements()).empty());
}
