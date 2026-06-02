#include <gtest/gtest.h>

#include "dss/storage/track_data_storage_format.h"

TEST(TrackDataStorageFormat, BuildsLegacyTrackDataLine) {
    Dss::Storage::TrackDataRecord record{};
    record.frameSeq = 123;
    record.timestamp = {
        .year = 2026, .month = 6, .day = 2, .hour = 3, .minute = 4, .second = 5, .millisecond = 6};
    record.fovCenterAe = {.x = 10.1234567F, .y = -20.5F};
    record.blobPosition = {.x = 3100.5F, .y = 3065.75F};
    record.opticCenter = {.x = 3072.0F, .y = 3072.0F};
    record.area = 10.0F;
    record.exposureTimeMilliseconds = 12.5;

    EXPECT_EQ(Dss::Storage::formatLegacyTrackDataLine(record),
              "123 2026-06-02  3:04:05.006 10.123457 -20.500000 28.5 6.25 "
              "10 17.3965 12.5 1350 13.2");
}

TEST(TrackDataStorageFormat, AllowsLegacyDefaultRangeAndMagnitudeOverrides) {
    Dss::Storage::TrackDataRecord record{};
    record.area = 2.0F;
    record.rangeMeters = 42.25;
    record.magnitude = -1.5;

    const auto line = Dss::Storage::formatLegacyTrackDataLine(record);

    EXPECT_TRUE(line.ends_with("2 0.695859 0 42.25 -1.5"));
}
