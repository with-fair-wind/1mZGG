#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/storage/track_data_storage_backend.h"
#include "dss/storage/track_data_storage_format.h"

namespace {

[[nodiscard]] auto tempTrackStorageDir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "dss_track_data_storage_backend_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

[[nodiscard]] auto readAllText(const std::filesystem::path& path) -> std::string {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] auto trackEvent() -> Dss::Core::TrackResultEvent {
    Dss::Core::MeasuredBlob blob{};
    blob.centroid = Dss::Core::Vec2f{3100.5F, 3065.75F};
    blob.area = 10.0F;
    blob.fovCenterAziModify = 10.1234567F;
    blob.fovCenterEleModify = -20.5F;

    Dss::Core::TargetFrameInfo frame{};
    frame.frameSeq = 123;
    frame.timestamp = {
        .year = 2026, .month = 6, .day = 2, .hour = 3, .minute = 4, .second = 5, .millisecond = 6};
    frame.fovCenterAe = Dss::Core::Vec2f{10.1234567F, -20.5F};
    frame.opticCenter = Dss::Core::Vec2f{3000.0F, 3001.0F};
    frame.exposureTime = 0.0125F;
    frame.measuredBlob = blob;
    frame.valid = true;

    Dss::Core::TargetInfo target{};
    target.targetId = "manual";
    target.living = true;
    target.frameInfos.push_back(frame);

    Dss::Core::TrackResultEvent event{};
    event.frameSeq = 123;
    event.targets.push_back(std::move(target));
    return event;
}

}  // namespace

TEST(TrackDataStorageBackend, RejectsEnqueueBeforeWorkerStarts) {
    auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());

    EXPECT_FALSE(backend.enqueueTrackResult(trackEvent()).has_value());
}

TEST(TrackDataStorageBackend, RejectsEnqueueWhenQueueIsFullAndCountsDrop) {
    auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir, 0);
    ASSERT_TRUE(backend.init(dir).has_value());
    ASSERT_TRUE(backend.start().has_value());

    EXPECT_FALSE(backend.enqueueTrackResult(trackEvent()).has_value());
    EXPECT_EQ(backend.droppedRequests(), 1U);

    backend.stop();
}
TEST(TrackDataStorageBackend, DrainsTrackDataWritesWhenStopped) {
    auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());
    ASSERT_TRUE(backend.start().has_value());

    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()).has_value());

    backend.stop();

    ASSERT_TRUE(std::filesystem::exists(backend.outputPath()));
    const auto text = readAllText(backend.outputPath());

    Dss::Storage::TrackDataRecord record{};
    record.frameSeq = 123;
    record.timestamp = {
        .year = 2026, .month = 6, .day = 2, .hour = 3, .minute = 4, .second = 5, .millisecond = 6};
    record.fovCenterAe = Dss::Core::Vec2f{10.1234567F, -20.5F};
    record.blobPosition = Dss::Core::Vec2f{3100.5F, 3065.75F};
    record.opticCenter = Dss::Core::Vec2f{3000.0F, 3001.0F};
    record.area = 10.0F;
    record.exposureTimeMilliseconds = 12.5;

    EXPECT_EQ(text, Dss::Storage::formatLegacyTrackDataLine(record) + "\n");
}
