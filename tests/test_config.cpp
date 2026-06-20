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
  "logging": {
    "enabled": true,
    "filePath": "./logs/production.log",
    "maxFileSizeBytes": 2048,
    "maxFiles": 4
  },
  "commNet": {
    "displayPort": { "portName": "COM1", "baudRate": 115200, "dataBits": 8, "stopBits": 1 },
    "exposurePort": { "portName": "COM2" },
    "masterControlPort": { "portName": "COM3" },
    "servoPort": { "portName": "COM4" },
    "cameraPort": "COM5",
    "imageSender": { "remoteIp": "127.0.0.1", "remotePort": 4123 },
    "exchange": { "remoteIp": "127.0.0.2", "remotePort": 5123 },
    "exchangeGxtc": { "remoteIp": "127.0.0.6", "remotePort": 6123 },
    "exchangeGdcl": { "remoteIp": "127.0.0.7", "remotePort": 6124 }
  },
  "optics": {
    "imageWidth": 32,
    "imageHeight": 16,
    "pixelScale": 0.125
  },
  "processing": {
    "thresholdSigma": 2.5,
    "diffThreshold": 24,
    "minArea": 4,
    "maxArea": 200,
    "displayLow": 120,
    "displayHigh": 4000
  },
  "observatory": {
    "id": "999",
    "longitude": 120.5,
    "latitude": 31.25,
    "altitude": 35.0
  },
  "tracking": {
    "thresholdLiving": 0.75,
    "autoDecide": false,
    "geoFullLeo": false,
    "geoRaThresholdArcsec": 5.4,
    "geoDecThresholdArcsec": 3.0,
    "geoRaSpeedThresholdArcsec": 10.0,
    "geoDecSpeedThresholdArcsec": 6.0
  }
})";
    }

    auto& config = Dss::Core::Config::instance();
    const auto loaded = config.load(path);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(config.paths().dataRoot, std::filesystem::path("./data"));
    EXPECT_TRUE(config.logging().enabled);
    EXPECT_EQ(config.logging().filePath, std::filesystem::path("./logs/production.log"));
    EXPECT_EQ(config.logging().maxFileSizeBytes, 2048U);
    EXPECT_EQ(config.logging().maxFiles, 4U);
    EXPECT_EQ(config.commNet().displayPort.portName, "COM1");
    EXPECT_EQ(config.commNet().displayPort.baudRate, 115200);
    EXPECT_EQ(config.commNet().cameraPort, "COM5");
    EXPECT_EQ(config.commNet().imageSender.remoteIp, "127.0.0.1");
    EXPECT_EQ(config.commNet().imageSender.remotePort, 4123);
    EXPECT_EQ(config.commNet().exchange.remoteIp, "127.0.0.2");
    EXPECT_EQ(config.commNet().exchange.remotePort, 5123);
    EXPECT_EQ(config.commNet().exchangeGxtc.remoteIp, "127.0.0.6");
    EXPECT_EQ(config.commNet().exchangeGxtc.remotePort, 6123);
    EXPECT_EQ(config.commNet().exchangeGdcl.remoteIp, "127.0.0.7");
    EXPECT_EQ(config.commNet().exchangeGdcl.remotePort, 6124);
    EXPECT_EQ(config.commNet().heartbeat.localPort, 15361);
    EXPECT_EQ(config.commNet().heartbeat.remoteIp, "0.0.0.0");
    EXPECT_EQ(config.commNet().heartbeat.remotePort, 15362);
    EXPECT_EQ(config.optics().imageWidth, 32);
    EXPECT_EQ(config.optics().imageHeight, 16);
    EXPECT_FLOAT_EQ(config.optics().pixelScale, 0.125f);
    EXPECT_DOUBLE_EQ(config.processing().thresholdSigma, 2.5);
    EXPECT_EQ(config.processing().diffThreshold, 24);
    EXPECT_EQ(config.processing().minArea, 4);
    EXPECT_EQ(config.processing().maxArea, 200);
    EXPECT_EQ(config.processing().displayLow, 120);
    EXPECT_EQ(config.processing().displayHigh, 4000);
    EXPECT_EQ(config.observatory().id, "999");
    EXPECT_DOUBLE_EQ(config.observatory().longitude, 120.5);
    EXPECT_DOUBLE_EQ(config.observatory().latitude, 31.25);
    EXPECT_DOUBLE_EQ(config.observatory().altitude, 35.0);
    EXPECT_FLOAT_EQ(config.trackingSettings().thresholdLiving, 0.75f);
    EXPECT_FALSE(config.trackingSettings().autoDecide);
    EXPECT_FALSE(config.trackingSettings().geoFullLeo);
    EXPECT_DOUBLE_EQ(config.trackingSettings().geoRaThresholdArcsec, 5.4);
    EXPECT_DOUBLE_EQ(config.trackingSettings().geoDecThresholdArcsec, 3.0);
    EXPECT_DOUBLE_EQ(config.trackingSettings().geoRaSpeedThresholdArcsec, 10.0);
    EXPECT_DOUBLE_EQ(config.trackingSettings().geoDecSpeedThresholdArcsec, 6.0);

    const auto saved = config.save();
    ASSERT_TRUE(saved.has_value()) << saved.error();

    {
        std::ifstream savedInput(path);
        const auto savedJson = nlohmann::json::parse(savedInput);
        EXPECT_EQ(savedJson.at("paths").at("dataRoot").get<std::string>(), "./data");
        EXPECT_TRUE(savedJson.at("logging").at("enabled").get<bool>());
        EXPECT_EQ(savedJson.at("logging").at("filePath").get<std::string>(),
                  "./logs/production.log");
        EXPECT_EQ(savedJson.at("logging").at("maxFileSizeBytes").get<std::size_t>(), 2048U);
        EXPECT_EQ(savedJson.at("logging").at("maxFiles").get<std::size_t>(), 4U);
        EXPECT_EQ(savedJson.at("commNet").at("displayPort").at("baudRate").get<int>(), 115200);
        EXPECT_EQ(savedJson.at("commNet").at("exchangeGxtc").at("remotePort").get<int>(), 6123);
        EXPECT_EQ(savedJson.at("commNet").at("exchangeGdcl").at("remotePort").get<int>(), 6124);
        EXPECT_FALSE(savedJson.at("tracking").at("autoDecide").get<bool>());
        EXPECT_FALSE(savedJson.at("tracking").at("geoFullLeo").get<bool>());
        EXPECT_DOUBLE_EQ(savedJson.at("processing").at("thresholdSigma").get<double>(), 2.5);
        EXPECT_EQ(savedJson.at("processing").at("diffThreshold").get<int>(), 24);
        EXPECT_EQ(savedJson.at("processing").at("displayLow").get<int>(), 120);
        EXPECT_EQ(savedJson.at("observatory").at("id").get<std::string>(), "999");
        EXPECT_DOUBLE_EQ(savedJson.at("tracking").at("geoRaThresholdArcsec").get<double>(), 5.4);
        EXPECT_DOUBLE_EQ(savedJson.at("tracking").at("geoDecThresholdArcsec").get<double>(), 3.0);
        EXPECT_DOUBLE_EQ(savedJson.at("tracking").at("geoRaSpeedThresholdArcsec").get<double>(),
                         10.0);
        EXPECT_DOUBLE_EQ(savedJson.at("tracking").at("geoDecSpeedThresholdArcsec").get<double>(),
                         6.0);
    }

    std::filesystem::remove(path);
}

TEST(ConfigTest, DerivesDataExchangeEndpointsFromLegacyExchange) {
    const auto path = std::filesystem::current_path() / "dss_config_legacy_exchange_test.json";
    {
        std::ofstream output(path);
        output << R"({
  "commNet": {
    "exchange": {
      "localIp": "127.0.0.1",
      "localPort": 10002,
      "remoteIp": "127.0.0.2",
      "remotePort": 10002
    }
  }
})";
    }

    auto& config = Dss::Core::Config::instance();
    const auto loaded = config.load(path);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(config.commNet().exchangeGxtc.localIp, "127.0.0.1");
    EXPECT_EQ(config.commNet().exchangeGxtc.localPort, 10002);
    EXPECT_EQ(config.commNet().exchangeGxtc.remoteIp, "127.0.0.2");
    EXPECT_EQ(config.commNet().exchangeGxtc.remotePort, 10002);
    EXPECT_EQ(config.commNet().exchangeGdcl.localIp, "127.0.0.1");
    EXPECT_EQ(config.commNet().exchangeGdcl.localPort, 10003);
    EXPECT_EQ(config.commNet().exchangeGdcl.remoteIp, "127.0.0.2");
    EXPECT_EQ(config.commNet().exchangeGdcl.remotePort, 10003);

    std::filesystem::remove(path);
}
TEST(ConfigTest, RejectsNegativeLogRotationValues) {
    const auto path = std::filesystem::current_path() / "dss_config_invalid_logging_test.json";
    {
        std::ofstream output(path);
        output << R"({"logging":{"maxFileSizeBytes":-1,"maxFiles":-2}})";
    }

    auto& config = Dss::Core::Config::instance();
    const auto loaded = config.load(path);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(config.logging().maxFileSizeBytes, 10U * 1024U * 1024U);
    EXPECT_EQ(config.logging().maxFiles, 5U);

    std::filesystem::remove(path);
}

TEST(ConfigTest, UsesSafeDefaultsForInvalidProcessingValues) {
    const auto path = std::filesystem::temp_directory_path() / "dss_invalid_processing.json";
    {
        std::ofstream output(path);
        output
            << R"({"processing":{"thresholdSigma":-1,"diffThreshold":-2,"minArea":200000,"maxArea":100,"displayLow":5000,"displayHigh":100}})";
    }

    auto& config = Dss::Core::Config::instance();
    ASSERT_TRUE(config.load(path).has_value());
    EXPECT_DOUBLE_EQ(config.processing().thresholdSigma, 3.0);
    EXPECT_EQ(config.processing().diffThreshold, 20);
    EXPECT_EQ(config.processing().minArea, 3);
    EXPECT_EQ(config.processing().maxArea, 100000);
    EXPECT_EQ(config.processing().displayLow, 0);
    EXPECT_EQ(config.processing().displayHigh, 16384);

    std::filesystem::remove(path);
}
