#include <cstdint>

#include <gtest/gtest.h>

#include "dss/app/runtime_diagnostics.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"

TEST(RuntimeDiagnostics, AggregatesBackendCountersAndErrorEvents) {
    Dss::App::RuntimeDiagnostics::MessageBus bus;
    std::uint64_t processingDropped = 2;
    std::uint64_t imageWrites = 3;
    std::uint64_t trackFailures = 4;
    Dss::App::RuntimeDiagnostics diagnostics(
        bus, {
                 .processingDroppedFrames = [&] { return processingDropped; },
                 .imageSuccessfulWrites = [&] { return imageWrites; },
                 .imageFailedWrites = [] { return std::uint64_t{1}; },
                 .imageDroppedRequests = [] { return std::uint64_t{5}; },
                 .trackSuccessfulWrites = [] { return std::uint64_t{6}; },
                 .trackFailedWrites = [&] { return trackFailures; },
                 .trackDroppedRequests = [] { return std::uint64_t{7}; },
             });

    bus.emit(Dss::Core::NetworkTransmissionErrorEvent{"gxtc", "closed", 10});
    bus.emit(Dss::Core::SerialFrameErrorEvent{"display", "bad", 20, 10, 0, 0});
    bus.emit(Dss::Core::SerialDecodeErrorEvent{"display", "bad", "year", 0, 0});
    bus.emit(Dss::Core::StorageWriteErrorEvent{"image", "frame.raw", "disk"});

    const auto snapshot = diagnostics.snapshot();
    EXPECT_EQ(snapshot.processingDroppedFrames, 2U);
    EXPECT_EQ(snapshot.imageSuccessfulWrites, 3U);
    EXPECT_EQ(snapshot.imageFailedWrites, 1U);
    EXPECT_EQ(snapshot.imageDroppedRequests, 5U);
    EXPECT_EQ(snapshot.trackSuccessfulWrites, 6U);
    EXPECT_EQ(snapshot.trackFailedWrites, 4U);
    EXPECT_EQ(snapshot.trackDroppedRequests, 7U);
    EXPECT_EQ(snapshot.networkErrors, 1U);
    EXPECT_EQ(snapshot.serialErrors, 2U);
    EXPECT_EQ(snapshot.storageErrors, 1U);
}
