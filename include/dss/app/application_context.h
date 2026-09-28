#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "dss/core/config/config.h"
#include "dss/core/event/message_bus.h"
#include "dss/core/service/service_registry.h"

namespace Dss::App {

/// 应用程序上下文，管理服务注册、事件总线和生命周期
class ApplicationContext {
public:
    using MessageBus = Dss::Core::MessageBus;  ///< 跨线程应用消息总线类型

    /// 构造应用程序上下文
    ApplicationContext();
    /// 析构时解除日志与事件总线的绑定
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&) = delete;
    ApplicationContext& operator=(ApplicationContext&&) = delete;

    /** @brief 获取应用事件总线。 @return 应用事件总线的引用。 */
    [[nodiscard]] auto bus() -> MessageBus&;
    /** @brief 获取服务注册表。 @return 服务注册表的引用。 */
    [[nodiscard]] auto registry() -> Dss::Core::ServiceRegistry&;
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
     * @brief 显式停止所有后台 worker 并清理订阅。
     *
     * 在 UI 线程先取消并等待 UI 发起的后台任务，再调用本方法，最后销毁 UI 对象。
     * 保留全部服务及订阅直到生产者 stop/join 完成，然后清理订阅与注册表。
     * 外部 shared_ptr 不影响停止动作；调用方不得并发重新启动服务。幂等，可重复调用。
     */
    void shutdown();

private:
    MessageBus m_bus;                                       ///< 应用内消息总线
    Dss::Core::ServiceRegistry m_registry;                  ///< 服务注册表
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接，随上下文析构自动取消
    std::function<void()> m_stopServices;  ///< 组合根定义的停止顺序，执行期间保留服务所有权。
};

}  // namespace Dss::App
