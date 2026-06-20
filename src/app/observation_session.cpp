#include "dss/app/observation_session.h"

#include <format>

namespace Dss::App {
namespace {

[[nodiscard]] bool isValidTime(const Dss::Core::TimeOfDay& time) {
    return time.hour >= 0 && time.hour < 24 && time.minute >= 0 && time.minute < 60 &&
           time.second >= 0 && time.second < 60;
}

[[nodiscard]] auto secondsSinceMidnight(const Dss::Core::TimeOfDay& time) -> int {
    return time.hour * 3600 + time.minute * 60 + time.second;
}

[[nodiscard]] auto formatTimestamp(std::chrono::sys_days date, const Dss::Core::TimeOfDay& time)
    -> std::string {
    const auto calendar = std::chrono::year_month_day{date};
    return std::format("{:04}{:02}{:02}{:02}{:02}{:02}", static_cast<int>(calendar.year()),
                       static_cast<unsigned>(calendar.month()),
                       static_cast<unsigned>(calendar.day()), time.hour, time.minute, time.second);
}

}  // namespace

auto makeObservationSession(const Dss::Core::MasterControlEvent& command,
                            std::string_view observatoryId, std::chrono::sys_days observationDate)
    -> std::expected<ObservationSession, std::string> {
    if (observatoryId.empty()) {
        return std::unexpected("observatory id cannot be empty");
    }
    if (!isValidTime(command.start) || !isValidTime(command.end)) {
        return std::unexpected("observation session contains an invalid time");
    }

    auto endDate = observationDate;
    if (secondsSinceMidnight(command.end) < secondsSinceMidnight(command.start)) {
        endDate += std::chrono::days{1};
    }

    const auto searchMode = command.targetId == 0U;
    const auto startTime = formatTimestamp(observationDate, command.start);
    const auto targetId = searchMode ? std::string{} : std::to_string(command.targetId);
    const auto targetSegment = searchMode ? std::string{"NNNNNN"} : targetId;

    ObservationSession session{};
    session.id = startTime + "_" + targetSegment + "_" + std::string{observatoryId};
    session.naming.startTime = startTime;
    session.naming.endTime = formatTimestamp(endDate, command.end);
    session.naming.taskId = std::to_string(command.taskId);
    session.naming.targetId = targetId;
    session.naming.observatoryId = observatoryId;
    session.naming.imageFormat = "raw";
    session.naming.searchMode = searchMode;
    session.source = ObservationSessionSource::MasterControl;
    return session;
}

}  // namespace Dss::App
