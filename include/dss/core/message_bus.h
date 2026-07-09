#pragma once

#include "dss/core/event_bus.h"

namespace Dss::Core {

/**
 * @brief 应用内统一消息总线类型。
 *
 * 该别名集中定义事件总线的锁策略，避免各模块重复展开底层模板类型。
 */
using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

}  // namespace Dss::Core
