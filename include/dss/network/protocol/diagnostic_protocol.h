#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace Dss::Network {

/// 各子系统诊断状态快照
struct DiagnosticStatus {
    bool displayCommOk = true;   ///< 显示通信是否正常
    bool exposureCommOk = true;  ///< 曝光通信是否正常
    bool cameraOk = true;        ///< 相机是否正常
    bool storageOk = true;       ///< 存储是否正常
    bool reservedOk = true;      ///< 预留项是否正常
};

/**
 * @brief 将诊断状态编码为 JSON 字符串
 * @param status 诊断状态快照
 * @return JSON 格式字符串（键为 DPS001–DPS004）
 */
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
