#pragma once

#include "dss/core/event_bus.h"
#include "dss/core/service_registry.h"

namespace Dss::Ui {

/**
 * @brief UI 子 ViewModel 共享的后端上下文。
 *
 * 该结构只保存事件总线与服务注册表引用，不拥有任何后端对象生命周期。
 */
struct UiServiceContext {
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名。

    MessageBus& bus;                       ///< 应用事件总线。
    Dss::Core::ServiceRegistry& registry;  ///< 应用服务注册表。
};

}  // namespace Dss::Ui
