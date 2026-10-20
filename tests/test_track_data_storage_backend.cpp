#include <filesystem>
#include <fstream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#include "dss/storage/format/track_data_storage_format.h"

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
        .year = 2026,
        .month = 6,
        .day = 2,
        .hour = 3,
        .minute = 4,
        .second = 5,
        .millisecond = 6,
    };
    frame.fovCenterAe = Dss::Core::Vec2f{10.1234567F, -20.5F};
    frame.opticCenter = Dss::Core::Vec2f{3000.0F, 3001.0F};
    frame.exposureTime = 0.0125F;
    frame.measuredBlob = std::move(blob);
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

TEST(TrackDataStorageBackend, ArchivesFinalValidMeasurementAndIgnoresEmptySnapshot) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    auto event = trackEvent();
    event.targets.front().living = false;
    ASSERT_TRUE(backend.enqueueTrackResult(event));
    ASSERT_TRUE(backend.enqueueTrackResult({124, {}}));
    event.targets.front().frameInfos.back().valid = false;
    ASSERT_TRUE(backend.enqueueTrackResult(event));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
    std::ifstream input(backend.outputPath());
    std::string line;
    ASSERT_TRUE(static_cast<bool>(std::getline(input, line)));
    EXPECT_FALSE(line.empty());
    EXPECT_FALSE(static_cast<bool>(std::getline(input, line)));
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
        .year = 2026,
        .month = 6,
        .day = 2,
        .hour = 3,
        .minute = 4,
        .second = 5,
        .millisecond = 6,
    };
    record.fovCenterAe = Dss::Core::Vec2f{10.1234567F, -20.5F};
    record.blobPosition = Dss::Core::Vec2f{3100.5F, 3065.75F};
    record.opticCenter = Dss::Core::Vec2f{3000.0F, 3001.0F};
    record.area = 10.0F;
    record.exposureTimeMilliseconds = 12.5;

    EXPECT_EQ(text, Dss::Storage::formatLegacyTrackDataLine(record) + "\n");
}
TEST(TrackDataStorageBackend, WritesConfiguredGaeSessionFile) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir).has_value());

    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "20260619080000";
    naming.targetId = "42";
    naming.observatoryId = "7";
    ASSERT_TRUE(backend.configureSession(naming).has_value());
    ASSERT_TRUE(backend.start().has_value());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()).has_value());
    backend.stop();

    EXPECT_EQ(backend.outputPath(), dir / "GAE" / "20260619080000_42_7.GAE");
    ASSERT_TRUE(std::filesystem::exists(backend.outputPath()));
    EXPECT_FALSE(readAllText(backend.outputPath()).empty());
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.failedWrites(), 0U);
}

TEST(TrackDataStorageBackend, RejectsOldGenerationAfterSessionSwitch) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    Dss::Storage::ImageStorageNaming naming{};
    naming.startTime = "first";
    naming.targetId = "1";
    ASSERT_TRUE(backend.configureSession(naming));
    ASSERT_TRUE(backend.start());
    const auto generation = backend.sessionGeneration();
    backend.requestStop();
    EXPECT_FALSE(backend.configureSession(naming));
    backend.stop();
    naming.startTime = "second";
    ASSERT_TRUE(backend.configureSession(naming));
    ASSERT_TRUE(backend.start());
    EXPECT_FALSE(backend.enqueueTrackResult(trackEvent(), generation));
    EXPECT_TRUE(backend.enqueueTrackResult(trackEvent(), backend.sessionGeneration()));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
}

TEST(TrackDataStorageBackend, ReportsWriteFailureAndCanRestartAfterPathIsRepaired) {
    const auto dir = tempTrackStorageDir();
    Dss::Core::MessageBus bus;
    std::vector<Dss::Core::StorageWriteErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::StorageWriteErrorEvent>(
        [&errors](const auto& event) { errors.push_back(event); });
    Dss::Storage::TrackDataStorageBackend backend(dir);
    backend.setBus(&bus);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(std::filesystem::create_directory(backend.outputPath()));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().backend, "track_data_storage");
    EXPECT_EQ(errors.front().path, backend.outputPath().string());
    EXPECT_FALSE(errors.front().message.empty());
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.successfulWrites(), 0U);
    EXPECT_EQ(backend.resourceSnapshot().completedItems, 1U);
    EXPECT_EQ(backend.resourceSnapshot().queuedItems, 0U);
    EXPECT_EQ(backend.resourceSnapshot().activeItems, 0U);

    ASSERT_TRUE(std::filesystem::remove(backend.outputPath()));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.resourceSnapshot().completedItems, 2U);
    EXPECT_FALSE(readAllText(backend.outputPath()).empty());
}

TEST(TrackDataStorageBackend, WriterProfilingIsDisabledByDefault) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
    const auto timing = backend.writerProfile();
    EXPECT_EQ(timing.batches, 0U);
    EXPECT_EQ(std::accumulate(timing.total.begin(), timing.total.end(), std::uint64_t{0}), 0U);
}

TEST(TrackDataStorageBackend, WriterProfilingCountsFailureRecoveryAndAccumulatesAcrossStarts) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir, 1024, true);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(std::filesystem::create_directory(backend.outputPath()));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    const auto failed = backend.writerProfile();
    EXPECT_EQ(failed.batches, 1U);
    EXPECT_EQ(backend.failedWrites(), 1U);

    ASSERT_TRUE(std::filesystem::remove(backend.outputPath()));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    const auto completed = backend.writerProfile();
    EXPECT_EQ(completed.batches, 2U);
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_GT(std::accumulate(completed.total.begin(), completed.total.end(), std::uint64_t{0}),
              0U);
    for (std::size_t index = 0; index < completed.total.size(); ++index) {
        EXPECT_GE(completed.total[index], failed.total[index]);
        EXPECT_GE(completed.total[index], completed.maximum[index]);
    }
}

TEST(TrackDataStorageBackend, RecreatesRemovedOutputDirectoryBeforeWriting) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    ASSERT_TRUE(std::filesystem::remove(dir));
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.failedWrites(), 0U);
    EXPECT_FALSE(readAllText(backend.outputPath()).empty());
}

TEST(TrackDataStorageBackend, WriterProfileRecordsAttemptDuringExceptionUnwinding) {
    Dss::Storage::TrackWriterProfile profile;
    EXPECT_THROW(
        {
            Dss::Storage::TrackWriterProfile::Batch batch(&profile);
            batch.begin(Dss::Storage::TrackWriterPhase::Format);
            throw std::runtime_error("profile unwind");
        },
        std::runtime_error);
    EXPECT_EQ(profile.snapshot().batches, 1U);
}

#ifdef __linux__
TEST(TrackDataStorageBackend, ReportsBufferedCloseFailureOnFullDevice) {
    const auto dir = tempTrackStorageDir();
    Dss::Core::MessageBus bus;
    std::vector<Dss::Core::StorageWriteErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::StorageWriteErrorEvent>(
        [&errors](const auto& event) { errors.push_back(event); });
    Dss::Storage::TrackDataStorageBackend backend(dir, 1024, true);
    backend.setBus(&bus);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(std::filesystem::exists("/dev/full"));
    std::filesystem::create_symlink("/dev/full", backend.outputPath());
    ASSERT_TRUE(backend.start());
    // 单行小于流缓冲，真实 ENOSPC 可延迟到关闭/刷新时才出现。
    ASSERT_TRUE(backend.enqueueTrackResult(trackEvent()));
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 0U);
    EXPECT_EQ(backend.failedWrites(), 1U);
    EXPECT_EQ(backend.writerProfile().batches, 1U);
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().message, "failed to write or close track data file");
}
#endif

TEST(TrackDataStorageBackend, LosslessAdmissionHonorsCancellationAndCapturedSession) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir, 1);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    const auto generation = backend.activeSessionGeneration();
    ASSERT_TRUE(generation);
    std::stop_source cancelled;
    cancelled.request_stop();
    const auto cancelledWrite =
        backend.enqueueTrackResultBlocking(trackEvent(), cancelled.get_token(), *generation);
    ASSERT_FALSE(cancelledWrite);
    EXPECT_EQ(cancelledWrite.error(), Dss::Storage::WriteAdmissionError::Cancelled);
    ASSERT_TRUE(backend.enqueueTrackResultBlocking(trackEvent(), {}, *generation));
    ASSERT_TRUE(backend.waitUntilIdle({}));
    backend.requestStop();
    EXPECT_FALSE(backend.activeSessionGeneration());
    backend.stop();
    ASSERT_TRUE(backend.start());
    const auto stale = backend.enqueueTrackResultBlocking(trackEvent(), {}, *generation);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), Dss::Storage::WriteAdmissionError::StaleSession);
    backend.stop();
    EXPECT_EQ(backend.successfulWrites(), 1U);
    EXPECT_EQ(backend.droppedRequests(), 0U);
}

TEST(TrackDataStorageBackend, DisabledCapacityCannotBlockLosslessProducer) {
    const auto dir = tempTrackStorageDir();
    Dss::Storage::TrackDataStorageBackend backend(dir, 0);
    ASSERT_TRUE(backend.init(dir));
    ASSERT_TRUE(backend.start());
    const auto result =
        backend.enqueueTrackResultBlocking(trackEvent(), {}, backend.sessionGeneration());
    backend.stop();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Dss::Storage::WriteAdmissionError::CapacityExceeded);
    EXPECT_EQ(backend.droppedRequests(), 1U);
    EXPECT_EQ(backend.resourceSnapshot().waitingItems, 0U);
}
