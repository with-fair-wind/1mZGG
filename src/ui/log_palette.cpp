#include "dss/ui/support/log_palette.h"

namespace Dss::Ui {

auto logTextColor(Dss::Core::LogLevel level) -> QColor {
    switch (level) {
        case Dss::Core::LogLevel::Warning:
            return {"#9A6700"};
        case Dss::Core::LogLevel::Error:
            return {"#B42318"};
        case Dss::Core::LogLevel::Info:
            return {"#263238"};
    }
    return {"#263238"};
}

auto inferLogLevelFromText(const QString& text) -> Dss::Core::LogLevel {
    if (text.startsWith("[ERROR]")) {
        return Dss::Core::LogLevel::Error;
    }
    if (text.startsWith("[WARN]")) {
        return Dss::Core::LogLevel::Warning;
    }
    return Dss::Core::LogLevel::Info;
}

auto logTextColorForEntry(const QString& text) -> QColor {
    return logTextColor(inferLogLevelFromText(text));
}

}  // namespace Dss::Ui
