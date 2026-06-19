#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "dss/core/config.h"
#include "dss/ui/settings_view_model.h"

TEST(SettingsViewModel, ValidatesAndPersistsProductionSettings) {
    const auto path = std::filesystem::temp_directory_path() / "dss_settings_view_model.json";
    {
        std::ofstream output(path);
        output << R"({"paths":{"dataRoot":"./data"}})";
    }
    auto& config = Dss::Core::Config::instance();
    ASSERT_TRUE(config.load(path).has_value());
    Dss::Ui::SettingsViewModel viewModel;

    auto snapshot = viewModel.snapshot();
    snapshot.dataRoot = "D:/DSS_Data";
    snapshot.logEnabled = true;
    snapshot.logFilePath = "D:/DSS_Data/logs/dss.log";
    snapshot.logMaxFileSizeBytes = 4096U;
    snapshot.logMaxFiles = 3U;
    snapshot.imageWidth = 2048;
    snapshot.imageHeight = 1024;
    snapshot.pixelScale = 0.5;

    const auto saved = viewModel.save(snapshot);
    ASSERT_TRUE(saved.has_value()) << saved.error().toStdString();
    EXPECT_EQ(config.paths().dataRoot, std::filesystem::path("D:/DSS_Data"));
    EXPECT_EQ(config.logging().maxFiles, 3U);
    EXPECT_EQ(config.optics().imageWidth, 2048);

    snapshot.logMaxFiles = 0U;
    EXPECT_FALSE(viewModel.save(snapshot).has_value());
    std::filesystem::remove(path);
}
