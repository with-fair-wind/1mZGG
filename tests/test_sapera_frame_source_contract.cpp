#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/sapera_frame_source.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"

namespace {

class FakeSaperaSession final : public Dss::Acquisition::ISaperaCaptureSession {
public:
    auto initialize(FrameCallback callback)
        -> std::expected<Dss::Acquisition::SaperaFrameGeometry, std::string> override {
        frameCallback = std::move(callback);
        if (!initializeError.empty()) {
            return std::unexpected(initializeError);
        }
        initialized = true;
        return geometry;
    }

    auto start() -> std::expected<void, std::string> override {
        if (!startError.empty()) {
            return std::unexpected(startError);
        }
        running = true;
        return {};
    }

    void stop() override {
        running = false;
        ++stopCount;
    }

    [[nodiscard]] bool isRunning() const override {
        return running;
    }

    void emit(std::span<const uint16_t> pixels) {
        frameCallback(pixels);
    }

    Dss::Acquisition::SaperaFrameGeometry geometry{3U, 2U};
    std::string initializeError;
    std::string startError;
    bool initialized = false;
    bool running = false;
    int stopCount = 0;
    FrameCallback frameCallback;
};

}  // namespace

TEST(SaperaFrameSourceContract, CopiesSdkBufferBeforeCallbackReturns) {
    auto session = std::make_unique<FakeSaperaSession>();
    auto* sessionView = session.get();
    Dss::Acquisition::SaperaFrameSource source({}, std::move(session));
    std::vector<Dss::Processing::FramePacket> packets;
    source.setFrameCallback(
        [&](Dss::Processing::FramePacket packet) { packets.push_back(std::move(packet)); });

    ASSERT_TRUE(source.init().has_value());
    ASSERT_TRUE(source.startCapture().has_value());
    std::vector<uint16_t> sdkBuffer{1, 2, 3, 4, 5, 6};
    sessionView->emit(sdkBuffer);
    sdkBuffer.assign(sdkBuffer.size(), 99U);

    ASSERT_EQ(packets.size(), 1U);
    EXPECT_EQ(packets[0].frameSeq, 0U);
    EXPECT_EQ(packets[0].width, 3U);
    EXPECT_EQ(packets[0].height, 2U);
    EXPECT_EQ(packets[0].rawImage, (std::vector<uint16_t>{1, 2, 3, 4, 5, 6}));
}

TEST(SaperaFrameSourceContract, ConvertsBackendErrorsAndPublishesAcquisitionEvent) {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    std::vector<Dss::Core::AcquisitionErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::AcquisitionErrorEvent>(
        [&](const auto& event) { errors.push_back(event); });
    auto session = std::make_unique<FakeSaperaSession>();
    session->startError = "transfer start failed";
    Dss::Acquisition::SaperaFrameSource source({}, std::move(session), &bus);

    ASSERT_TRUE(source.init().has_value());
    const auto started = source.startCapture();

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error(), "transfer start failed");
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors[0].source, "sapera");
    EXPECT_EQ(errors[0].message, "transfer start failed");
    EXPECT_FALSE(source.isRunning());
}

TEST(SaperaFrameSourceContract, RejectsWrongSdkBufferSize) {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    std::vector<Dss::Core::AcquisitionErrorEvent> errors;
    auto connection = bus.subscribe<Dss::Core::AcquisitionErrorEvent>(
        [&](const auto& event) { errors.push_back(event); });
    auto session = std::make_unique<FakeSaperaSession>();
    auto* sessionView = session.get();
    Dss::Acquisition::SaperaFrameSource source({}, std::move(session), &bus);
    ASSERT_TRUE(source.init().has_value());
    ASSERT_TRUE(source.startCapture().has_value());

    const std::vector<uint16_t> incomplete{1, 2};
    sessionView->emit(incomplete);

    EXPECT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].message.find("pixel count"), std::string::npos);
}

#ifndef DSS_HAS_SAPERA
TEST(SaperaFrameSourceContract, DisabledBuildFailsCleanlyWithoutSdkSession) {
    Dss::Acquisition::SaperaFrameSource source;

    const auto initialized = source.init();

    ASSERT_FALSE(initialized.has_value());
    EXPECT_NE(initialized.error().find("disabled"), std::string::npos);
    EXPECT_FALSE(source.isRunning());
}
#endif
