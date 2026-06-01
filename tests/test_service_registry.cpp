#include "dss/core/service_registry.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{

class IChannel
{
public:
    virtual ~IChannel() = default;

    [[nodiscard]] virtual auto name() const -> std::string_view = 0;
};

class RecordingChannel final : public IChannel
{
public:
    explicit RecordingChannel(std::string channelName) : m_name(std::move(channelName)) {}

    [[nodiscard]] auto name() const -> std::string_view override { return m_name; }

private:
    std::string m_name;
};

} // namespace

TEST(ServiceRegistry, KeepsNamedImplementationsOfSameInterface)
{
    Dss::Core::ServiceRegistry registry;
    auto display = std::make_shared<RecordingChannel>("display");
    auto exposure = std::make_shared<RecordingChannel>("exposure");

    registry.registerService<IChannel>("display", display);
    registry.registerService<IChannel>("exposure", exposure);

    EXPECT_TRUE(registry.has<IChannel>("display"));
    EXPECT_TRUE(registry.has<IChannel>("exposure"));
    EXPECT_EQ(registry.get<IChannel>("display")->name(), "display");
    EXPECT_EQ(registry.get<IChannel>("exposure")->name(), "exposure");
    EXPECT_EQ(registry.tryGet<IChannel>("missing"), nullptr);
}

TEST(ServiceRegistry, PreservesUnnamedServiceCompatibility)
{
    Dss::Core::ServiceRegistry registry;
    auto channel = std::make_shared<RecordingChannel>("default");

    registry.registerService<IChannel>(channel);

    EXPECT_TRUE(registry.has<IChannel>());
    EXPECT_EQ(registry.get<IChannel>()->name(), "default");
    EXPECT_EQ(registry.tryGet<IChannel>()->name(), "default");
}
