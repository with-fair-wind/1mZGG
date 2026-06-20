#pragma once

#include <chrono>
#include <expected>
#include <string>
#include <string_view>

#include "dss/core/events.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::App {

enum class ObservationSessionSource {
    MasterControl,
    LocalReplay,
};

struct ObservationSession {
    std::string id;
    Dss::Storage::ImageStorageNaming naming;
    ObservationSessionSource source = ObservationSessionSource::MasterControl;
};

[[nodiscard]] auto makeObservationSession(const Dss::Core::MasterControlEvent& command,
                                          std::string_view observatoryId,
                                          std::chrono::sys_days observationDate)
    -> std::expected<ObservationSession, std::string>;

}  // namespace Dss::App
