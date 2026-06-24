#pragma once

#include <chrono>
#include <expected>
#include <string>
#include <string_view>

#include "dss/core/events.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::App {

/// @brief 观测会话的触发来源。
enum class ObservationSessionSource {
    MasterControl,  ///< 由主控指令创建
    LocalReplay,    ///< 由本地回放创建
};

/// @brief 一次观测任务的稳定标识与存储命名信息。
struct ObservationSession {
    std::string id;                           ///< 会话唯一标识
    Dss::Storage::ImageStorageNaming naming;  ///< 图像与跟踪数据的存储命名参数
    ObservationSessionSource source = ObservationSessionSource::MasterControl;  ///< 会话触发来源
};

/**
 * @brief 根据主控任务指令创建观测会话。
 * @param command 包含任务编号、目标编号和起止时间的主控指令。
 * @param observatoryId 台站编号。
 * @param observationDate 观测日期。
 * @return 成功时返回会话；指令时间或标识无效时返回错误描述。
 */
[[nodiscard]] auto makeObservationSession(const Dss::Core::MasterControlEvent& command,
                                          std::string_view observatoryId,
                                          std::chrono::sys_days observationDate)
    -> std::expected<ObservationSession, std::string>;

}  // namespace Dss::App
