#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "dss/core/config.h"
#include "dss/core/event_bus.h"
#include "dss/core/service_host.h"
#include "dss/core/service_registry.h"

namespace Dss::App {

/// 应用程序上下文，管理服务注册、事件总线和生命周期
class ApplicationContext {
public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 跨线程应用消息总线类型

    /// 构造应用程序上下文
    ApplicationContext();
    /// 析构时停止全部服务并解除日志与事件总线的绑定
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&) = delete;
    ApplicationContext& operator=(ApplicationContext&&) = delete;

    /** @brief 获取应用事件总线。 @return 应用事件总线的引用。 */
    [[nodiscard]] auto bus() -> MessageBus&;
    /** @brief 获取服务注册表。 @return 服务注册表的引用。 */
    [[nodiscard]] auto registry() -> Dss::Core::ServiceRegistry&;
    /** @brief 获取服务生命周期管理器。 @return 服务宿主的引用。 */
    [[nodiscard]] auto services() -> Dss::Core::ServiceHost&;

    /// 将全局日志实例绑定到本上下文的事件总线
    void wireLogger();

    /**
     * @brief 加载系统配置
     * @param configPath JSON 配置文件路径
     * @return 失败时返回包含错误信息的 std::expected
     */
    auto loadConfig(const std::filesystem::path& configPath) -> std::expected<void, std::string>;

    /**
     * @brief 注册通信、采集、处理与存储等业务服务
     * @note 仅在定义了 DSS_BUILD_APP 时执行实际注册
     */
    void registerCommunicationServices();

    /**
     * @brief 启动已注册的全部服务
     * @return 失败时返回包含错误信息的 std::expected
     */
    auto startServices() -> std::expected<void, std::string>;

    /// 停止全部服务（不抛出异常）
    void stopServices() noexcept;

private:
    MessageBus m_bus;                                       ///< 应用内消息总线
    Dss::Core::ServiceRegistry m_registry;                  ///< 服务注册表
    Dss::Core::ServiceHost m_services;                      ///< 服务生命周期管理器
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接，随上下文析构自动取消
};

}  // namespace Dss::App
