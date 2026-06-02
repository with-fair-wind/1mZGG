#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace Dss::Network {

struct DiagnosticStatus {
    bool displayCommOk = true;
    bool exposureCommOk = true;
    bool cameraOk = true;
    bool storageOk = true;
    bool reservedOk = true;
};

[[nodiscard]] inline auto buildDiagnosticStatusJson(const DiagnosticStatus& status) -> std::string {
    const nlohmann::json payload{
        {"DPS001", status.displayCommOk && status.exposureCommOk ? 1 : 0},
        {"DPS002", status.cameraOk ? 1 : 0},
        {"DPS003", status.storageOk ? 1 : 0},
        {"DPS004", status.reservedOk ? 1 : 0},
    };
    return payload.dump();
}

}  // namespace Dss::Network
