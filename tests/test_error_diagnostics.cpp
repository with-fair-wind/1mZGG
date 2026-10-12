#include <gtest/gtest.h>

#include "dss/core/event/events.h"
#include "dss/network/endpoint/error_diagnostics.h"

TEST(ErrorDiagnostics, AggregatesCommunicationAndStorageFailures) {
    Dss::Network::ErrorDiagnostics::MessageBus bus;
    Dss::Network::ErrorDiagnostics diagnostics(bus);

    bus.emit(Dss::Core::SerialFrameErrorEvent{.channel = "display", .message = {}});
    bus.emit(Dss::Core::SerialDecodeErrorEvent{.channel = "exposure", .message = {}, .field = {}});
    bus.emit(
        Dss::Core::StorageWriteErrorEvent{.backend = "image_storage", .path = {}, .message = {}});
    bus.emit(Dss::Core::NetworkTransmissionErrorEvent{.channel = "data_exchange", .message = {}});

    const auto status = diagnostics.statusSnapshot();
    EXPECT_FALSE(status.displayCommOk);
    EXPECT_FALSE(status.exposureCommOk);
    EXPECT_FALSE(status.storageOk);
    EXPECT_FALSE(status.reservedOk);
    EXPECT_TRUE(status.cameraOk);
}
