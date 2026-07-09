#pragma once

#include <QColor>
#include <QString>

#include "dss/core/events.h"

namespace Dss::Ui {

/// @brief 根据日志级别返回 UI 文本颜色。
/// @param level 日志级别。
/// @return 用于日志页渲染的文本颜色。
[[nodiscard]] auto logTextColor(Dss::Core::LogLevel level) -> QColor;

/// @brief 从已格式化日志文本前缀推断日志级别。
/// @param text 日志页展示文本。
/// @return 推断出的日志级别；无法识别时返回 Info。
[[nodiscard]] auto inferLogLevelFromText(const QString& text) -> Dss::Core::LogLevel;

/// @brief 根据日志页展示文本返回 UI 文本颜色。
/// @param text 日志页展示文本。
/// @return 用于日志页渲染的文本颜色。
[[nodiscard]] auto logTextColorForEntry(const QString& text) -> QColor;

}  // namespace Dss::Ui
