#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/network/data_exchange.h"

namespace {

using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

}  // namespace

TEST(DataExchange, ReportsGxtcSendFailureWhenChannelIsClosed) {
    MessageBus bus;
    std::vector<Dss::Core::NetworkTransmissionErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>(
        [&errors](const Dss::Core::NetworkTransmissionErrorEvent& event) {
            errors.push_back(event);
        });
    Dss::Network::DataExchange exchange(bus);

    const std::vector<uint8_t> payload{0x01U, 0x02U, 0x03U};
    const auto result = exchange.sendGxtc(payload);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "gxtc UDP channel is not open or send failed");
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().channel, "gxtc");
    EXPECT_EQ(errors.front().message, result.error());
    EXPECT_EQ(errors.front().attemptedBytes, payload.size());
}

TEST(DataExchange, ReportsGdclEncodedSendFailureWhenChannelIsClosed) {
    MessageBus bus;
    std::vector<Dss::Core::NetworkTransmissionErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>(
        [&errors](const Dss::Core::NetworkTransmissionErrorEvent& event) {
            errors.push_back(event);
        });
    Dss::Network::DataExchange exchange(bus);

    const Dss::Network::GdclMeasurement measurement{
        .jms1970Centiseconds = 123,
        .measureStatus = 1,
        .targetId = 42,
    };
    const auto result = exchange.sendGdcl(measurement);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "gdcl UDP channel is not open or send failed");
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().channel, "gdcl");
    EXPECT_EQ(errors.front().attemptedBytes, Dss::Network::GdclPacketSize);
}
