#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "dss/app/track_result_data_exchange_bridge.h"
#include "dss/core/event_bus.h"

namespace {

using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

struct CapturedExchange {
    std::vector<Dss::Network::GxtcMetadata> gxtcMetadata;
    std::vector<std::vector<Dss::Network::GxtcTarget>> gxtcTargets;
    std::vector<Dss::Network::GdclMeasurement> gdclMeasurements;
};

[[nodiscard]] auto makeTarget(std::string targetId, bool valid) -> Dss::Core::TargetInfo {
    Dss::Core::MeasuredBlob blob{};
    blob.id = targetId;
    blob.centroid = Dss::Core::Vec2f{100.0F, 200.0F};
    blob.dn = 4096.75F;
    blob.posAe = Dss::Core::Vec2f{1.0F, 2.0F};

    Dss::Core::TargetFrameInfo frame{};
    frame.timestamp = {
        .year = 1970,
        .month = 1,
        .day = 2,
        .hour = 0,
        .minute = 0,
        .second = 1,
        .millisecond = 230,
    };
    frame.frameSeq = 9;
    frame.measuredBlob = blob;
    frame.posZxdw = Dss::Core::Vec2f{1.25F, -2.5F};
    frame.posTwdw = Dss::Core::Vec2f{-0.1F, 0.2F};
    frame.magnitude = -1.25F;
    frame.valid = valid;

    Dss::Core::TargetInfo target{};
    target.targetId = std::move(targetId);
    target.predictedSpdAe = Dss::Core::Vec2f{0.125F, -0.25F};
    target.frameInfos.push_back(frame);
    return target;
}

[[nodiscard]] auto makeTrackEvent() -> Dss::Core::TrackResultEvent {
    Dss::Core::TrackResultEvent event{};
    event.frameSeq = 9;
    event.targets.push_back(makeTarget("geo-41", false));
    event.targets.push_back(makeTarget("geo-42", true));
    return event;
}

}  // namespace

TEST(TrackResultDataExchangeBridge, SendsGxtcForAllTargetsAndGdclForMasterTarget) {
    MessageBus bus;
    CapturedExchange captured;
    Dss::App::TrackResultDataExchangeBridge bridge(
        bus,
        [&captured](const Dss::Network::GxtcMetadata& metadata,
                    std::span<const Dss::Network::GxtcTarget> targets) {
            captured.gxtcMetadata.push_back(metadata);
            captured.gxtcTargets.emplace_back(targets.begin(), targets.end());
        },
        [&captured](const Dss::Network::GdclMeasurement& measurement) {
            captured.gdclMeasurements.push_back(measurement);
        });

    bus.emit(Dss::Core::MasterControlEvent{.track = true, .targetId = 42});
    bus.emit(makeTrackEvent());

    ASSERT_EQ(captured.gxtcMetadata.size(), 1U);
    EXPECT_EQ(captured.gxtcMetadata.front().jms1970Centiseconds, 8640123);
    EXPECT_EQ(captured.gxtcMetadata.front().measureStatus, 1U);
    EXPECT_EQ(captured.gxtcMetadata.front().targetStatus, 1U);

    ASSERT_EQ(captured.gxtcTargets.size(), 1U);
    ASSERT_EQ(captured.gxtcTargets.front().size(), 2U);
    EXPECT_EQ(captured.gxtcTargets.front()[0].targetId, 41);
    EXPECT_EQ(captured.gxtcTargets.front()[1].targetId, 42);
    EXPECT_EQ(captured.gxtcTargets.front()[1].azimuthSpeedArcsecPerSecond, 450);

    ASSERT_EQ(captured.gdclMeasurements.size(), 1U);
    EXPECT_EQ(captured.gdclMeasurements.front().targetId, 42);
    EXPECT_EQ(captured.gdclMeasurements.front().measureStatus, 1U);
    EXPECT_EQ(captured.gdclMeasurements.front().dn, 4096);
    EXPECT_EQ(captured.gdclMeasurements.front().magnitudeCentivalue, -125);
}

TEST(TrackResultDataExchangeBridge, SkipsGdclWhenMasterTargetIsUnknown) {
    MessageBus bus;
    CapturedExchange captured;
    Dss::App::TrackResultDataExchangeBridge bridge(
        bus,
        [&captured](const Dss::Network::GxtcMetadata& metadata,
                    std::span<const Dss::Network::GxtcTarget> targets) {
            captured.gxtcMetadata.push_back(metadata);
            captured.gxtcTargets.emplace_back(targets.begin(), targets.end());
        },
        [&captured](const Dss::Network::GdclMeasurement& measurement) {
            captured.gdclMeasurements.push_back(measurement);
        });

    bus.emit(makeTrackEvent());

    EXPECT_EQ(captured.gxtcMetadata.size(), 1U);
    EXPECT_TRUE(captured.gdclMeasurements.empty());
}
