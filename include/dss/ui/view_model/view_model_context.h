#pragma once

#include "dss/core/event/message_bus.h"
#include "dss/core/service/service_registry.h"

namespace Dss::Ui {

/**
 * @brief UI 子 ViewModel 共享的后端上下文。
 *
 * 该结构只保存事件总线与服务注册表引用，不拥有任何后端对象生命周期。
 */
struct UiServiceContext {
    using MessageBus = Dss::Core::MessageBus;  ///< 事件总线类型别名。

    // 借用服务，引用在
    // ViewModel 生命周期内有效。
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    MessageBus& bus;  ///< 应用事件总线。
    // 借用注册表，上下文不支持重绑定。
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    Dss::Core::ServiceRegistry& registry;  ///< 应用服务注册表。
};

}  // namespace Dss::Ui
