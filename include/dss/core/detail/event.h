#pragma once

#include <algorithm>
#include <cstddef>
#include <exception>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "dss/core/detail/event_bus_primitives.h"

namespace Dss::Evt {
namespace detail {

/**
 * @brief 事件核心：COW 快照 + 锁策略
 * @tparam Handler 处理器类型
 * @tparam LockPolicy 锁策略类型
 * @tparam Args 事件参数类型
 */
template <typename Handler, typename LockPolicy, typename... Args>
class EventCore {
public:
    using Id = std::size_t;  ///< 处理器槽位 ID
    struct Slot {
        Id id;            ///< 槽位 ID
        Handler handler;  ///< 处理器
    };
    using HandlerList = std::vector<Slot>;                 ///< 处理器列表
    using StoredArgs = std::tuple<std::decay_t<Args>...>;  ///< 待派发参数元组

    EventCore() : m_handlers(std::make_shared<HandlerList>()) {}

    /// 获取处理器列表快照（共享指针，COW 安全）
    std::shared_ptr<HandlerList> snapshot() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers;
    }

    /// 添加处理器，返回槽位 ID
    Id addHandler(Handler handler) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cowDetach();
        Id id = m_nextId++;
        m_handlers->push_back(Slot{id, std::move(handler)});
        return id;
    }

    /// 按 ID 移除处理器
    void removeHandler(Id id) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cowDetach();
        auto& items = *m_handlers;
        std::erase_if(items, [id](const Slot& s) { return s.id == id; });
    }

    /// 清除所有处理器
    void clearHandlers() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_handlers = std::make_shared<HandlerList>();
    }

    /// 获取当前处理器数量
    [[nodiscard]] std::size_t handlerCount() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers ? m_handlers->size() : 0;
    }

    /**
     * @brief 将事件参数加入待派发队列
     * @tparam UArgs 参数类型
     */
    template <typename... UArgs>
    void pushPending(UArgs&&... uargs) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.emplace_back(std::decay_t<Args>(std::forward<UArgs>(uargs))...);
    }

    /// 取出并清空待派发队列
    std::vector<StoredArgs> takePending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        return std::move(m_pending);
    }

    /// 待派发事件数量
    [[nodiscard]] std::size_t pendingCount() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_pending.size();
    }

    /// 清空待派发队列
    void clearPending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.clear();
    }

private:
    /// COW 分离：确保 m_handlers 独占可写
    void cowDetach() {
        if (!m_handlers) {
            m_handlers = std::make_shared<HandlerList>();
        } else if (m_handlers.use_count() > 1) {
            m_handlers = std::make_shared<HandlerList>(*m_handlers);
        }
    }

    mutable LockPolicy m_lock;                ///< 锁策略
    std::shared_ptr<HandlerList> m_handlers;  ///< 处理器列表（COW）
    Id m_nextId = 1;                          ///< 下一个槽位 ID
    std::vector<StoredArgs> m_pending;        ///< 待派发参数队列
};

}  // namespace detail

/**
 * @brief 事件（前向声明）
 * @tparam Signature 函数签名
 * @tparam CombinerT 返回值组合器
 * @tparam LockPolicy 锁策略
 */
template <typename Signature, template <typename> class CombinerT = CollectAll,
          typename LockPolicy = NoLock>
class Event;

/**
 * @brief 无返回值事件
 * @tparam Args 事件参数类型
 * @tparam CombinerT 组合器（void 事件不使用）
 * @tparam LockPolicy 锁策略
 */
template <typename... Args, template <typename> class CombinerT, typename LockPolicy>
class Event<void(Args...), CombinerT, LockPolicy> : private detail::MovePolicy<true> {
public:
    using Handler = Delegate<void(Args...)>;  ///< 处理器类型
    using Id = std::size_t;                   ///< 槽位 ID

private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;

public:
    Event() : m_core(std::make_shared<Core>()) {}

    /**
     * @brief 订阅事件
     * @param handler 处理器委托
     * @return 作用域连接，析构时自动取消订阅
     */
    ScopedConnection subscribe(Handler handler) {
        ensureCore();
        Id id = m_core->addHandler(std::move(handler));
        std::weak_ptr<Core> weak = m_core;
        return ScopedConnection([weak, id]() {
            if (auto core = weak.lock()) {
                core->removeHandler(id);
            }
        });
    }

    /**
     * @brief 从可调用对象订阅事件
     * @tparam F 可调用对象类型
     */
    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    /// 清除所有处理器
    void clear() {
        if (m_core) {
            m_core->clearHandlers();
        }
    }

    /// 处理器数量
    [[nodiscard]] std::size_t size() const {
        return m_core ? m_core->handlerCount() : 0;
    }
    /// 是否无处理器
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    /**
     * @brief 同步派发事件
     * @param args 事件参数
     */
    void emit(Args... args) const {
        if (!m_core) {
            return;
        }
        auto snap = m_core->snapshot();
        if (!snap || snap->empty()) {
            return;
        }
        detail::forEachSafe(snap->begin(), snap->end(), [&](const auto& slot) {
            if (slot.handler.valid()) {
                slot.handler(static_cast<Args>(args)...);
            }
        });
    }

    /// 同 emit，函数调用语法糖
    void operator()(Args... args) const {
        emit(std::forward<Args>(args)...);
    }

    /**
     * @brief 异步投递事件（入队，需 flush 派发）
     * @tparam UArgs 参数类型
     */
    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensureCore();
        m_core->pushPending(std::forward<UArgs>(uargs)...);
    }

    /// 派发所有待处理事件
    void flush() {
        if (!m_core) {
            return;
        }
        auto batch = m_core->takePending();
        detail::forEachSafe(batch.begin(), batch.end(), [this](auto& stored) {
            std::apply([this](auto&... args) { emit(static_cast<Args>(args)...); }, stored);
        });
    }

    /// 待派发事件数量
    [[nodiscard]] std::size_t pendingCount() const {
        return m_core ? m_core->pendingCount() : 0;
    }

    /// 清空待派发队列
    void clearPending() {
        if (m_core) {
            m_core->clearPending();
        }
    }

private:
    /// 延迟创建核心对象
    void ensureCore() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    std::shared_ptr<Core> m_core;  ///< 事件核心
};

/**
 * @brief 有返回值事件（带组合器）
 * @tparam R 返回值类型
 * @tparam Args 事件参数类型
 * @tparam CombinerT 返回值组合器
 * @tparam LockPolicy 锁策略
 */
template <typename R, typename... Args, template <typename> class CombinerT, typename LockPolicy>
class Event<R(Args...), CombinerT, LockPolicy> : private detail::MovePolicy<true> {
public:
    using Handler = Delegate<R(Args...)>;               ///< 处理器类型
    using Id = std::size_t;                             ///< 槽位 ID
    using Combiner = CombinerT<R>;                      ///< 组合器类型
    using ResultType = typename Combiner::result_type;  ///< 组合结果类型

private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;
    using Adapter =
        detail::CombinerAdapter<Combiner, R, detail::HasAccumulator<Combiner>::value>;

public:
    Event() : m_core(std::make_shared<Core>()) {}

    /**
     * @brief 订阅事件
     * @param handler 处理器委托
     * @return 作用域连接
     */
    ScopedConnection subscribe(Handler handler) {
        ensureCore();
        Id id = m_core->addHandler(std::move(handler));
        std::weak_ptr<Core> weak = m_core;
        return ScopedConnection([weak, id]() {
            if (auto core = weak.lock()) {
                core->removeHandler(id);
            }
        });
    }

    /**
     * @brief 从可调用对象订阅事件
     * @tparam F 可调用对象类型
     */
    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    /// 清除所有处理器
    void clear() {
        if (m_core) {
            m_core->clearHandlers();
        }
    }

    /// 处理器数量
    [[nodiscard]] std::size_t size() const {
        return m_core ? m_core->handlerCount() : 0;
    }
    /// 是否无处理器
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    /**
     * @brief 同步派发事件并组合返回值
     * @param args 事件参数
     * @return 组合器合并后的结果
     */
    ResultType emit(Args... args) const {
        if (!m_core) {
            return emptyResult();
        }
        auto snap = m_core->snapshot();
        if (!snap || snap->empty()) {
            return emptyResult();
        }
        typename Adapter::Accumulator acc;
        acc.reserve(snap->size());
        std::exception_ptr firstEx;
        for (const auto& slot : *snap) {
            if (slot.handler.valid()) {
                try {
                    acc.add(slot.handler(static_cast<Args>(args)...));
                    if (acc.shouldStop()) {
                        break;
                    }
                } catch (...) {
                    if (!firstEx) {
                        firstEx = std::current_exception();
                    }
                }
            }
        }
        if (firstEx) {
            std::rethrow_exception(firstEx);
        }
        return acc.finalize();
    }

    /// 同 emit，函数调用语法糖
    ResultType operator()(Args... args) const {
        return emit(std::forward<Args>(args)...);
    }

    /**
     * @brief 异步投递事件
     * @tparam UArgs 参数类型
     */
    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensureCore();
        m_core->pushPending(std::forward<UArgs>(uargs)...);
    }

    /// 派发所有待处理事件，返回各次 emit 的组合结果
    std::vector<ResultType> flush() {
        if (!m_core) {
            return {};
        }
        auto batch = m_core->takePending();
        std::vector<ResultType> allResults;
        allResults.reserve(batch.size());
        detail::forEachSafe(batch.begin(), batch.end(), [&](auto& stored) {
            allResults.push_back(std::apply(
                [this](auto&... args) { return emit(static_cast<Args>(args)...); }, stored));
        });
        return allResults;
    }

    /// 待派发事件数量
    [[nodiscard]] std::size_t pendingCount() const {
        return m_core ? m_core->pendingCount() : 0;
    }

    /// 清空待派发队列
    void clearPending() {
        if (m_core) {
            m_core->clearPending();
        }
    }

private:
    /// 延迟创建核心对象
    void ensureCore() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    /// 无处理器时的默认返回值
    static ResultType emptyResult() {
        return detail::EmptyResultAdapter<Combiner, detail::HasEmptyResult<Combiner>::value>::get();
    }

    std::shared_ptr<Core> m_core;  ///< 事件核心
};

/**
 * @brief 基于消息类型的发布/订阅总线
 * @tparam LockPolicy 锁策略（默认无锁）
 */

}  // namespace Dss::Evt
