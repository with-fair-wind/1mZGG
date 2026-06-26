#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/network/error_diagnostics.h"

TEST(ErrorDiagnostics, AggregatesCommunicationAndStorageFailures) {
    Dss::Network::ErrorDiagnostics::MessageBus bus;
    Dss::Network::ErrorDiagnostics diagnostics(bus);

    bus.emit(Dss::Core::SerialFrameErrorEvent{.channel = "display"});
    bus.emit(Dss::Core::SerialDecodeErrorEvent{.channel = "exposure"});
    bus.emit(Dss::Core::StorageWriteErrorEvent{.backend = "image_storage"});
    bus.emit(Dss::Core::NetworkTransmissionErrorEvent{.channel = "data_exchange"});

    const auto status = diagnostics.statusSnapshot();
    EXPECT_FALSE(status.displayCommOk);
    EXPECT_FALSE(status.exposureCommOk);
    EXPECT_FALSE(status.storageOk);
    EXPECT_FALSE(status.reservedOk);
    EXPECT_TRUE(status.cameraOk);
}
