#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "dss/core/i_service.h"

namespace Dss::Core {

/// 服务生命周期管理器，按序启动和逆序停止已注册服务
class ServiceHost {
public:
    /**
     * @brief 添加服务（仅在未运行且尚未启动任何服务时允许）
     * @param service 服务实例
     * @return 添加成功返回 true
     */
    [[nodiscard]] bool add(std::shared_ptr<IService> service);

    /// 获取已注册服务数量
    [[nodiscard]] auto serviceCount() const -> std::size_t;

    /**
     * @brief 按注册顺序启动所有服务
     * @return 任一服务启动失败时返回错误信息，并回滚已启动的服务
     */
    auto startAll() -> std::expected<void, std::string>;

    /// 逆序停止所有已启动的服务
    void stopAll() noexcept;

private:
    std::vector<std::shared_ptr<IService>> m_services;  ///< 已注册服务列表
    std::size_t m_startedCount = 0;                     ///< 已成功启动的服务数量
    bool m_running = false;                             ///< 是否处于运行状态
};

}  // namespace Dss::Core
