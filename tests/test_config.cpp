#include "dss/core/config.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

TEST(ConfigTest, LoadsIniWithoutQt)
{
    const auto path = std::filesystem::current_path() / "dss_config_test.ini";
    {
        std::ofstream output(path);
        output << "[Path]\n";
        output << "DataRoot=./data\n";
        output << "CCFFile=./data/ccf.dat\n";
        output << "KernelFile=./kernels\n\n";
        output << "[CommNetSettings]\n";
        output << "DisplayPort=COM1\n";
        output << "ExposurePort=COM2\n";
        output << "MasterControlPort=COM3\n";
        output << "ServoPort=COM4\n";
        output << "CameraPort=COM5\n";
        output << "IPImageTrans=127.0.0.1\n";
        output << "PortImageTrans=4123\n\n";
        output << "[OpticSettings]\n";
        output << "ImageWidth=32\n";
        output << "ImageHeight=16\n";
        output << "PixelScale=0.125\n\n";
        output << "[TrackSettings]\n";
        output << "ThresholdLiving=0.75\n";
        output << "AutoDecide=false\n";
    }

    auto& config = Dss::Core::Config::instance();
    const auto loaded = config.load(path);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(config.paths().dataRoot, std::filesystem::path("./data"));
    EXPECT_EQ(config.commNet().displayPort.portName, "COM1");
    EXPECT_EQ(config.commNet().cameraPort, "COM5");
    EXPECT_EQ(config.commNet().imageSender.remoteIp, "127.0.0.1");
    EXPECT_EQ(config.commNet().imageSender.remotePort, 4123);
    EXPECT_EQ(config.optics().imageWidth, 32);
    EXPECT_EQ(config.optics().imageHeight, 16);
    EXPECT_FLOAT_EQ(config.optics().pixelScale, 0.125f);
    EXPECT_FLOAT_EQ(config.trackingSettings().thresholdLiving, 0.75f);
    EXPECT_FALSE(config.trackingSettings().autoDecide);

    std::filesystem::remove(path);
}
