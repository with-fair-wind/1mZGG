#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "dss/acquisition/frame_source_coordinator.h"

namespace {

class FakeFrameSource final : public Dss::Acquisition::IFrameSource {
public:
    auto init() -> std::expected<void, std::string> override {
        ++initCount;
        if (!initError.empty()) {
            return std::unexpected(initError);
        }
        initialized = true;
        return {};
    }

    void start() override {
        ++startCount;
        running = true;
    }

    void stop() override {
        ++stopCount;
        running = false;
    }

    void setFrameCallback(FrameCallback callback) override {
        frameCallback = std::move(callback);
    }

    [[nodiscard]] bool isRunning() const override {
        return running;
    }

    [[nodiscard]] auto frameWidth() const -> uint32_t override {
        return width;
    }

    [[nodiscard]] auto frameHeight() const -> uint32_t override {
        return height;
    }

    std::string initError;
    bool initialized = false;
    bool running = false;
    int initCount = 0;
    int startCount = 0;
    int stopCount = 0;
    uint32_t width = 640;
    uint32_t height = 480;
    FrameCallback frameCallback;
};

}  // namespace

TEST(FrameSourceCoordinator, RunsOnlyTheSelectedSource) {
    auto replay = std::make_shared<FakeFrameSource>();
    auto live = std::make_shared<FakeFrameSource>();
    Dss::Acquisition::FrameSourceCoordinator coordinator;
    ASSERT_TRUE(coordinator.registerSource(Dss::Acquisition::FrameSourceMode::Replay, replay));
    ASSERT_TRUE(coordinator.registerSource(Dss::Acquisition::FrameSourceMode::Live, live));

    coordinator.start();
    ASSERT_TRUE(replay->running);
    ASSERT_FALSE(live->running);

    const auto switched = coordinator.selectSource(Dss::Acquisition::FrameSourceMode::Live);

    ASSERT_TRUE(switched.has_value()) << switched.error();
    EXPECT_FALSE(replay->running);
    EXPECT_TRUE(live->running);
    EXPECT_EQ(coordinator.activeMode(), Dss::Acquisition::FrameSourceMode::Live);
    EXPECT_EQ(replay->stopCount, 1);
    EXPECT_EQ(live->startCount, 1);
}

TEST(FrameSourceCoordinator, KeepsRunningSourceWhenTargetInitializationFails) {
    auto replay = std::make_shared<FakeFrameSource>();
    auto live = std::make_shared<FakeFrameSource>();
    live->initError = "camera unavailable";
    Dss::Acquisition::FrameSourceCoordinator coordinator;
    ASSERT_TRUE(coordinator.registerSource(Dss::Acquisition::FrameSourceMode::Replay, replay));
    ASSERT_TRUE(coordinator.registerSource(Dss::Acquisition::FrameSourceMode::Live, live));
    coordinator.start();

    const auto switched = coordinator.selectSource(Dss::Acquisition::FrameSourceMode::Live);

    ASSERT_FALSE(switched.has_value());
    EXPECT_EQ(switched.error(), "camera unavailable");
    EXPECT_TRUE(replay->running);
    EXPECT_FALSE(live->running);
    EXPECT_EQ(coordinator.activeMode(), Dss::Acquisition::FrameSourceMode::Replay);
    EXPECT_EQ(replay->stopCount, 0);
}

TEST(FrameSourceCoordinator, DoesNotStartRegisteredSourcesByDefault) {
    auto replay = std::make_shared<FakeFrameSource>();
    Dss::Acquisition::FrameSourceCoordinator coordinator;

    ASSERT_TRUE(coordinator.registerSource(Dss::Acquisition::FrameSourceMode::Replay, replay));

    EXPECT_FALSE(coordinator.isRunning());
    EXPECT_FALSE(replay->running);
    EXPECT_EQ(replay->initCount, 0);
    EXPECT_EQ(replay->startCount, 0);
}
