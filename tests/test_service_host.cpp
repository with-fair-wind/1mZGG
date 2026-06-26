#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/service_host.h"

namespace {

class RecordingService final : public Dss::Core::IService {
public:
    RecordingService(std::string serviceName, std::vector<std::string>& events,
                     bool failStart = false)
        : m_name(std::move(serviceName)), m_events(events), m_failStart(failStart) {}

    [[nodiscard]] auto name() const -> std::string_view override {
        return m_name;
    }

    auto start() -> std::expected<void, std::string> override {
        m_events.push_back("start:" + m_name);
        if (m_failStart) {
            return std::unexpected("boom");
        }
        return {};
    }

    void stop() noexcept override {
        m_events.push_back("stop:" + m_name);
    }

private:
    std::string m_name;
    std::vector<std::string>& m_events;
    bool m_failStart = false;
};

}  // namespace

TEST(ServiceHost, StartsAndStopsServicesInLifecycleOrder) {
    std::vector<std::string> events;
    Dss::Core::ServiceHost host;
    ASSERT_TRUE(host.add(std::make_shared<RecordingService>("first", events)));
    ASSERT_TRUE(host.add(std::make_shared<RecordingService>("second", events)));

    ASSERT_TRUE(host.startAll());
    host.stopAll();

    EXPECT_EQ(events, (std::vector<std::string>{"start:first", "start:second", "stop:second",
                                                "stop:first"}));
}

TEST(ServiceHost, StopsAlreadyStartedServicesWhenStartFails) {
    std::vector<std::string> events;
    Dss::Core::ServiceHost host;
    ASSERT_TRUE(host.add(std::make_shared<RecordingService>("first", events)));
    ASSERT_TRUE(host.add(std::make_shared<RecordingService>("second", events, true)));

    const auto result = host.startAll();

    ASSERT_FALSE(result);
    EXPECT_EQ(events, (std::vector<std::string>{"start:first", "start:second", "stop:first"}));
}

TEST(ServiceHost, RejectsServicesAddedAfterStart) {
    std::vector<std::string> events;
    Dss::Core::ServiceHost host;
    ASSERT_TRUE(host.add(std::make_shared<RecordingService>("first", events)));
    ASSERT_TRUE(host.startAll());

    const auto added = host.add(std::make_shared<RecordingService>("late", events));

    EXPECT_FALSE(added);
    EXPECT_EQ(host.serviceCount(), 1U);
    host.stopAll();
}
