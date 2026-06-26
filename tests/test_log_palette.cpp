#include <QColor>
#include <QString>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/ui/log_palette.h"

TEST(LogPaletteTest, MapsLogLevelsToDistinctUiColors) {
    const auto infoColor = Dss::Ui::logTextColor(Dss::Core::LogLevel::Info);
    const auto warningColor = Dss::Ui::logTextColor(Dss::Core::LogLevel::Warning);
    const auto errorColor = Dss::Ui::logTextColor(Dss::Core::LogLevel::Error);

    EXPECT_NE(infoColor, warningColor);
    EXPECT_NE(warningColor, errorColor);
    EXPECT_NE(infoColor, errorColor);
    EXPECT_EQ(warningColor, QColor("#9A6700"));
    EXPECT_EQ(errorColor, QColor("#B42318"));
}

TEST(LogPaletteTest, InfersLogLevelFromRenderedTextPrefix) {
    EXPECT_EQ(Dss::Ui::inferLogLevelFromText("[WARN] serial frame error"),
              Dss::Core::LogLevel::Warning);
    EXPECT_EQ(Dss::Ui::inferLogLevelFromText("[ERROR] udp send failed"),
              Dss::Core::LogLevel::Error);
    EXPECT_EQ(Dss::Ui::inferLogLevelFromText("[INFO] replay started"), Dss::Core::LogLevel::Info);
    EXPECT_EQ(Dss::Ui::inferLogLevelFromText("legacy message without prefix"),
              Dss::Core::LogLevel::Info);
}
