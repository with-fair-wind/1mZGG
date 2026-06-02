#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "dss/core/config.h"

TEST(ConfigTest, LoadsJsonWithoutQt) {
    const auto path = std::filesystem::current_path() / "dss_config_test.json";
    {
        std::ofstream output(path);
        output << R"({
  "paths": {
    "dataRoot": "./data",
    "ccfFile": "./data/ccf.dat",
    "kernelFile": "./kernels"
  },
  "commNet": {
    "displayPort": { "portName": "COM1", "baudRate": 115200, "dataBits": 8, "stopBits": 1 },
    "exposurePort": { "portName": "COM2" },
    "masterControlPort": { "portName": "COM3" },
    "servoPort": { "portName": "COM4" },
    "cameraPort": "COM5",
    "imageSender": { "remoteIp": "127.0.0.1", "remotePort": 4123 },
    "exchange": { "remoteIp": "127.0.0.2", "remotePort": 5123 }
  },
  "optics": {
    "imageWidth": 32,
    "imageHeight": 16,
    "pixelScale": 0.125
  },
  "observatory": {
    "longitude": 120.5,
    "latitude": 31.25,
    "altitude": 35.0
  },
  "tracking": {
    "thresholdLiving": 0.75,
    "autoDecide": false
  }
})";
    }

    auto& config = Dss::Core::Config::instance();
    const auto loaded = config.load(path);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(config.paths().dataRoot, std::filesystem::path("./data"));
    EXPECT_EQ(config.commNet().displayPort.portName, "COM1");
    EXPECT_EQ(config.commNet().displayPort.baudRate, 115200);
    EXPECT_EQ(config.commNet().cameraPort, "COM5");
    EXPECT_EQ(config.commNet().imageSender.remoteIp, "127.0.0.1");
    EXPECT_EQ(config.commNet().imageSender.remotePort, 4123);
    EXPECT_EQ(config.commNet().exchange.remoteIp, "127.0.0.2");
    EXPECT_EQ(config.commNet().exchange.remotePort, 5123);
    EXPECT_EQ(config.commNet().heartbeat.localPort, 15361);
    EXPECT_EQ(config.commNet().heartbeat.remoteIp, "0.0.0.0");
    EXPECT_EQ(config.commNet().heartbeat.remotePort, 15362);
    EXPECT_EQ(config.optics().imageWidth, 32);
    EXPECT_EQ(config.optics().imageHeight, 16);
    EXPECT_FLOAT_EQ(config.optics().pixelScale, 0.125f);
    EXPECT_DOUBLE_EQ(config.observatory().longitude, 120.5);
    EXPECT_DOUBLE_EQ(config.observatory().latitude, 31.25);
    EXPECT_DOUBLE_EQ(config.observatory().altitude, 35.0);
    EXPECT_FLOAT_EQ(config.trackingSettings().thresholdLiving, 0.75f);
    EXPECT_FALSE(config.trackingSettings().autoDecide);

    const auto saved = config.save();
    ASSERT_TRUE(saved.has_value()) << saved.error();

    {
        std::ifstream savedInput(path);
        const auto savedJson = nlohmann::json::parse(savedInput);
        EXPECT_EQ(savedJson.at("paths").at("dataRoot").get<std::string>(), "./data");
        EXPECT_EQ(savedJson.at("commNet").at("displayPort").at("baudRate").get<int>(), 115200);
        EXPECT_FALSE(savedJson.at("tracking").at("autoDecide").get<bool>());
    }

    std::filesystem::remove(path);
}
