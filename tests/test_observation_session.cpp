#include <chrono>

#include <gtest/gtest.h>

#include "dss/app/observation_session.h"

namespace {

constexpr auto kObservationDate =
    std::chrono::sys_days{std::chrono::year{2026} / std::chrono::June / 20};

}  // namespace

TEST(ObservationSession, MapsMasterControlTaskToTrackedSessionNaming) {
    const Dss::Core::MasterControlEvent command{
        .targetId = 42,
        .taskId = 7,
        .start = {.hour = 8, .minute = 30, .second = 0},
        .end = {.hour = 9, .minute = 45, .second = 6},
    };

    const auto session = Dss::App::makeObservationSession(command, "999", kObservationDate);

    ASSERT_TRUE(session.has_value()) << session.error();
    EXPECT_EQ(session->id, "20260620083000_42_999");
    EXPECT_EQ(session->naming.startTime, "20260620083000");
    EXPECT_EQ(session->naming.endTime, "20260620094506");
    EXPECT_EQ(session->naming.taskId, "7");
    EXPECT_EQ(session->naming.targetId, "42");
    EXPECT_EQ(session->naming.observatoryId, "999");
    EXPECT_FALSE(session->naming.searchMode);
}

TEST(ObservationSession, UsesNextDateForEndTimeAcrossMidnight) {
    const Dss::Core::MasterControlEvent command{
        .targetId = 42,
        .taskId = 7,
        .start = {.hour = 23, .minute = 59, .second = 58},
        .end = {.hour = 0, .minute = 5, .second = 0},
    };

    const auto session = Dss::App::makeObservationSession(command, "999", kObservationDate);

    ASSERT_TRUE(session.has_value()) << session.error();
    EXPECT_EQ(session->naming.startTime, "20260620235958");
    EXPECT_EQ(session->naming.endTime, "20260621000500");
}

TEST(ObservationSession, UsesSearchPlaceholderWhenTargetIsUnknown) {
    const Dss::Core::MasterControlEvent command{
        .targetId = 0,
        .taskId = 7,
        .start = {.hour = 8},
        .end = {.hour = 9},
    };

    const auto session = Dss::App::makeObservationSession(command, "999", kObservationDate);

    ASSERT_TRUE(session.has_value()) << session.error();
    EXPECT_TRUE(session->naming.searchMode);
    EXPECT_TRUE(session->naming.targetId.empty());
    EXPECT_EQ(session->id, "20260620080000_NNNNNN_999");
}

TEST(ObservationSession, RejectsInvalidTimeAndObservatory) {
    const Dss::Core::MasterControlEvent invalidTime{
        .start = {.hour = 24},
        .end = {.hour = 9},
    };
    const Dss::Core::MasterControlEvent validTime{
        .start = {.hour = 8},
        .end = {.hour = 9},
    };

    EXPECT_FALSE(Dss::App::makeObservationSession(invalidTime, "999", kObservationDate));
    EXPECT_FALSE(Dss::App::makeObservationSession(validTime, "", kObservationDate));
}
