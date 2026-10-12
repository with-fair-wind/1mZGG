#pragma once

#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "dss/core/event/events.h"
#include "dss/core/event/message_bus.h"

namespace Dss::Core {

/**
 * @brief 在后台线程顶层执行任务并把未处理异常转换为诊断事件。
 * @tparam F 无参数任务类型。
 * @param bus 用于发布失败事件的消息总线。
 * @param component 后台组件稳定名称。
 * @param task 待执行任务。
 * @note 诊断事件发布失败时向标准错误输出尽力报告；任何异常均不会逃出线程边界。
 */
template <typename F>
void runBackgroundTask(MessageBus& bus, std::string_view component, F&& task) noexcept {
    try {
        std::invoke(std::forward<F>(task));
    } catch (const std::exception& error) {
        try {
            bus.emit(BackgroundTaskErrorEvent{std::string(component), error.what()});
        } catch (...) {
            Dss::Core::reportEmergencyFailure("DSS: background diagnostic failed\n");
        }
    } catch (...) {
        try {
            bus.emit(BackgroundTaskErrorEvent{std::string(component), "unknown exception"});
        } catch (...) {
            Dss::Core::reportEmergencyFailure("DSS: background diagnostic failed\n");
        }
    }
}

}  // namespace Dss::Core
