#pragma once

#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace Dss::Ui {

/**
 * @brief 判断当前调用是否运行在指定 QObject 的所属线程。
 * @param object 待检查线程亲和性的 Qt 对象。
 * @return object 非空且当前线程等于 object 所属线程时返回 true。
 */
[[nodiscard]] inline auto isObjectThread(const QObject* object) -> bool {
    return object != nullptr && QThread::currentThread() == object->thread();
}

/**
 * @brief 将任务投递到 QObject 所属线程执行。
 *
 * 若当前已经在 object 所属线程内，任务会立即执行；否则通过 Qt queued connection
 * 投递到 object 的事件循环。该工具仅用于 UI 对象线程亲和性，不负责创建工作线程。
 *
 * @tparam Fn 可调用对象类型。
 * @param object 接收投递任务的 Qt 对象。
 * @param fn 需要在 object 所属线程执行的任务。
 */
template <typename Fn>
void invokeOnObjectThread(QObject* object, Fn&& fn) {
    if (object == nullptr) {
        return;
    }

    using Task = std::decay_t<Fn>;
    if (isObjectThread(object)) {
        std::invoke(std::forward<Fn>(fn));
        return;
    }

    auto task = std::make_shared<Task>(std::forward<Fn>(fn));
    QMetaObject::invokeMethod(
        object, [task = std::move(task)] { std::invoke(*task); }, Qt::QueuedConnection);
}

}  // namespace Dss::Ui
