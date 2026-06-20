#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dss/acquisition/serial_camera_controller.h"

namespace {

class FakeCameraSerialPort final : public Dss::Acquisition::ICameraSerialPort {
public:
    [[nodiscard]] bool isOpen() const override {
        return open;
    }

    [[nodiscard]] auto portName() const -> std::string_view override {
        return name;
    }

    auto write(std::span<const uint8_t> bytes) -> std::expected<void, std::string> override {
        if (!open) {
            return std::unexpected("port closed");
        }
        if (writes.size() == failAtWrite) {
            return std::unexpected("write failed");
        }
        writes.emplace_back(bytes.begin(), bytes.end());
        return {};
    }

    bool open = false;
    std::string name{"COM-test"};
    std::size_t failAtWrite = static_cast<std::size_t>(-1);
    std::vector<std::vector<uint8_t>> writes;
};

}  // namespace

TEST(SerialCameraController, SendsExistingProtocolBytesThroughNarrowPort) {
    auto port = std::make_shared<FakeCameraSerialPort>();
    port->open = true;
    Dss::Acquisition::SerialCameraController controller(port);

    ASSERT_TRUE(controller.sendGrab(true).has_value());
    ASSERT_EQ(port->writes.size(), 1U);
    const auto expectedGrab = Dss::Acquisition::buildGrabCommand(true);
    EXPECT_EQ(port->writes[0], std::vector<uint8_t>(expectedGrab.begin(), expectedGrab.end()));

    ASSERT_TRUE(controller.sendExposure(4.5F).has_value());
    const auto expectedExposure = Dss::Acquisition::buildExposureCommands(4.5F);
    ASSERT_EQ(port->writes.size(), 1U + expectedExposure.size());
    for (std::size_t index = 0; index < expectedExposure.size(); ++index) {
        EXPECT_EQ(port->writes[index + 1U], std::vector<uint8_t>(expectedExposure[index].begin(),
                                                                 expectedExposure[index].end()));
    }
}

TEST(SerialCameraController, ReportsClosedPortAndStopsAfterWriteFailure) {
    auto port = std::make_shared<FakeCameraSerialPort>();
    Dss::Acquisition::SerialCameraController controller(port);

    const auto closed = controller.sendGrab(false);
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error(), "camera serial port is not open");

    port->open = true;
    port->failAtWrite = 1U;
    const auto result = controller.sendGain(14.0F);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "write failed");
    EXPECT_EQ(port->writes.size(), 1U);
}

TEST(SerialCameraController, ConstructionDoesNotOpenHardware) {
    auto port = std::make_shared<FakeCameraSerialPort>();

    const Dss::Acquisition::SerialCameraController controller(port);

    EXPECT_FALSE(controller.isOpen());
    EXPECT_EQ(controller.portName(), "COM-test");
    EXPECT_TRUE(port->writes.empty());
}
