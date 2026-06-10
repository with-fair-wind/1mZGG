#pragma once

/// C++23 就绪的事件总线，基于项目 eventBus17.hpp 实现。
/// 本头文件在 Dss 命名空间内重新导出 Dss::Evt 组件。
/// 后续计划：将 SFINAE 升级为 concepts，std::function 升级为 std::move_only_function。

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

/// Qt 将 emit 定义为宏；此处必须取消定义，因为非 Qt 事件系统中 Event::emit() 是合法方法名。
#ifdef emit
#undef emit
#define DSS_EVT_RESTORE_EMIT
#endif

namespace Dss::Evt {

/// 无锁策略（单线程或外部同步场景）
struct NoLock {
    void lock() const noexcept {}
    void unlock() const noexcept {}
    void lock_shared() const noexcept {}
    void unlock_shared() const noexcept {}
};

/// 共享互斥锁策略（读写锁）
struct SharedMutexLock {
    mutable std::shared_mutex m_mutex;  ///< 底层读写锁
    void lock() const {
        m_mutex.lock();
    }
    void unlock() const {
        m_mutex.unlock();
    }
    void lock_shared() const {
        m_mutex.lock_shared();
    }
    void unlock_shared() const {
        m_mutex.unlock_shared();
    }
};

namespace detail {

/// 独占锁 RAII 守卫
template <typename L>
struct UniqueLockGuard {
    const L& lk;  ///< 锁策略引用
    explicit UniqueLockGuard(const L& l) : lk(l) {
        lk.lock();
    }
    ~UniqueLockGuard() {
        lk.unlock();
    }
    UniqueLockGuard(const UniqueLockGuard&) = delete;
    UniqueLockGuard& operator=(const UniqueLockGuard&) = delete;
};

/// 共享锁 RAII 守卫
template <typename L>
struct SharedLockGuard {
    const L& lk;  ///< 锁策略引用
    explicit SharedLockGuard(const L& l) : lk(l) {
        lk.lock_shared();
    }
    ~SharedLockGuard() {
        lk.unlock_shared();
    }
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
};

/**
 * @brief 移动策略：AllowMove 为 true 时允许移动
 * @tparam AllowMove 是否允许移动构造/赋值
 */
template <bool AllowMove>
struct MovePolicy {
    MovePolicy() = default;
    ~MovePolicy() = default;
    MovePolicy(const MovePolicy&) = delete;
    MovePolicy& operator=(const MovePolicy&) = delete;
    MovePolicy(MovePolicy&&) noexcept = default;
    MovePolicy& operator=(MovePolicy&&) noexcept = default;
};

template <>
struct MovePolicy<false> {
    MovePolicy() = default;
    ~MovePolicy() = default;
    MovePolicy(const MovePolicy&) = delete;
    MovePolicy& operator=(const MovePolicy&) = delete;
    MovePolicy(MovePolicy&&) = delete;
    MovePolicy& operator=(MovePolicy&&) = delete;
};

/**
 * @brief 安全遍历处理器列表，捕获并重新抛出首个异常
 * @tparam Iter 迭代器类型
 * @tparam Fn 回调函数类型
 */
template <typename Iter, typename Fn>
void forEachSafe(Iter begin, Iter end, Fn&& fn) {
    std::exception_ptr firstEx;
    for (auto it = begin; it != end; ++it) {
        try {
            fn(*it);
        } catch (...) {
            if (!firstEx) {
                firstEx = std::current_exception();
            }
        }
    }
    if (firstEx) {
        std::rethrow_exception(firstEx);
    }
}

template <typename C, typename R, typename = void>
struct HasAccumulator : std::false_type {};

template <typename C, typename R>
struct HasAccumulator<C, R, std::void_t<typename C::Accumulator>> : std::true_type {};

template <typename Combiner, typename R, bool HasAccum>
struct CombinerAdapter;

template <typename Combiner, typename R>
struct CombinerAdapter<Combiner, R, true> {
    using Accumulator = typename Combiner::Accumulator;
};

template <typename Combiner, typename R>
struct CombinerAdapter<Combiner, R, false> {
    struct Accumulator {
        std::vector<R> results;  ///< 收集的返回值列表
        void reserve(std::size_t n) {
            results.reserve(n);
        }
        void add(R&& val) {
            results.push_back(std::move(val));
        }
        bool shouldStop() const {
            return false;
        }
        typename Combiner::result_type finalize() {
            return Combiner::combine(std::move(results));
        }
    };
};

template <typename C, typename = void>
struct HasEmptyResult : std::false_type {};

template <typename C>
struct HasEmptyResult<C, std::void_t<decltype(C::emptyResult())>> : std::true_type {};

template <typename Combiner, bool Has>
struct EmptyResultAdapter;

template <typename Combiner>
struct EmptyResultAdapter<Combiner, true> {
    static typename Combiner::result_type get() {
        return Combiner::emptyResult();
    }
};

template <typename Combiner>
struct EmptyResultAdapter<Combiner, false> {
    [[noreturn]] static typename Combiner::result_type get() {
        throw std::logic_error(
            "Dss::Evt::Event::emit() called with no handlers on a Combiner that "
            "requires at least one");
    }
};

}  // namespace detail

/// 作用域连接，析构或 disconnect 时自动取消订阅
class ScopedConnection {
public:
    ScopedConnection() = default;

    /**
     * @brief 构造作用域连接
     * @param disconnectFn 断开订阅时调用的回调
     */
    explicit ScopedConnection(std::function<void()> disconnectFn)
        : m_disconnectFn(std::move(disconnectFn)), m_active(true) {}

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ScopedConnection(ScopedConnection&& other) noexcept {
        if (other.m_active.exchange(false, std::memory_order_acq_rel)) {
            m_disconnectFn = std::move(other.m_disconnectFn);
            m_active.store(true, std::memory_order_release);
        }
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnectNoexcept();
            if (other.m_active.exchange(false, std::memory_order_acq_rel)) {
                m_disconnectFn = std::move(other.m_disconnectFn);
                m_active.store(true, std::memory_order_release);
            } else {
                m_disconnectFn = nullptr;
            }
        }
        return *this;
    }

    ~ScopedConnection() noexcept {
        disconnectNoexcept();
    }

    /// 主动断开订阅
    void disconnect() {
        if (m_active.exchange(false, std::memory_order_acq_rel)) {
            if (m_disconnectFn) {
                auto fn = std::move(m_disconnectFn);
                fn();
            }
        }
    }

    /// 连接是否仍处于活跃状态
    [[nodiscard]] bool isActive() const {
        return m_active.load(std::memory_order_acquire);
    }

private:
    /// 不抛出异常的断开操作（供析构使用）
    void disconnectNoexcept() noexcept {
        try {
            disconnect();
        } catch (...) {
        }
    }

    std::function<void()> m_disconnectFn;  ///< 断开订阅回调
    std::atomic<bool> m_active{false};     ///< 连接是否活跃
};

/**
 * @brief 类型擦除的可调用委托
 * @tparam Signature 函数签名 R(Args...)
 */
template <typename Signature>
class Delegate;

template <typename R, typename... Args>
class Delegate<R(Args...)> {
public:
    Delegate() = default;
    explicit Delegate(std::function<R(Args...)> func) : m_fn(std::move(func)) {}

    /**
     * @brief 从可调用对象构造委托
     * @tparam F 可调用对象类型
     */
    template <typename F>
    static Delegate from(F&& func) {
        return Delegate(std::function<R(Args...)>(std::forward<F>(func)));
    }

    /// 从函数指针构造委托
    static Delegate from(R (*func)(Args...)) {
        return Delegate(std::function<R(Args...)>(func));
    }

    /**
     * @brief 从成员函数构造委托（非 const）
     * @tparam T 对象类型
     */
    template <typename T>
    static Delegate from(T* obj, R (T::*method)(Args...)) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    /**
     * @brief 从成员函数构造委托（const）
     * @tparam T 对象类型
     */
    template <typename T>
    static Delegate from(const T* obj, R (T::*method)(Args...) const) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    /// 调用委托
    R operator()(Args... args) const {
        assert(m_fn && "Delegate::operator() called on empty delegate");
        return m_fn(std::forward<Args>(args)...);
    }

    /// 委托是否有效（已绑定可调用对象）
    [[nodiscard]] bool valid() const {
        return static_cast<bool>(m_fn);
    }

private:
    std::function<R(Args...)> m_fn;  ///< 底层可调用对象
};

/**
 * @brief 组合器：收集所有处理器返回值
 * @tparam R 返回值类型
 */
template <typename R>
struct CollectAll {
    using result_type = std::vector<R>;  ///< 组合结果类型
    static result_type emptyResult() {
        return {};
    }
    static result_type combine(std::vector<R>&& results) {
        return std::move(results);
    }
};

/**
 * @brief 组合器：取最后一个处理器的返回值
 * @tparam R 返回值类型
 */
template <typename R>
struct LastValue {
    using result_type = R;  ///< 组合结果类型
    struct Accumulator {
        std::optional<R> lastValue;  ///< 最近一次返回值
        void reserve(std::size_t) {}
        void add(R&& val) {
            lastValue.emplace(std::move(val));
        }
        bool shouldStop() const {
            return false;
        }
        R finalize() {
            assert(lastValue.has_value());
            return std::move(*lastValue);
        }
    };
    static result_type combine(std::vector<R>&& results) {
        assert(!results.empty());
        return std::move(results.back());
    }
};

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
        Id id;              ///< 槽位 ID
        Handler handler;    ///< 处理器
    };
    using HandlerList = std::vector<Slot>;  ///< 处理器列表
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

    mutable LockPolicy m_lock;                      ///< 锁策略
    std::shared_ptr<HandlerList> m_handlers;        ///< 处理器列表（COW）
    Id m_nextId = 1;                                ///< 下一个槽位 ID
    std::vector<StoredArgs> m_pending;              ///< 待派发参数队列
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
    using Handler = Delegate<R(Args...)>;       ///< 处理器类型
    using Id = std::size_t;                     ///< 槽位 ID
    using Combiner = CombinerT<R>;              ///< 组合器类型
    using ResultType = typename Combiner::result_type;  ///< 组合结果类型

private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;
    using Adapter =
        detail::CombinerAdapter<Combiner, R, detail::HasAccumulator<Combiner, R>::value>;

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
template <typename LockPolicy = NoLock>
class BasicMessageBus : private detail::MovePolicy<std::is_same_v<LockPolicy, NoLock>> {
public:
    BasicMessageBus() = default;

    /**
     * @brief 订阅指定消息类型
     * @tparam Message 消息类型
     * @param func 消息处理函数
     * @return 作用域连接
     */
    template <typename Message>
    ScopedConnection subscribe(std::function<void(const Message&)> func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        return ensureChannel<Message>()->m_event.subscribe(std::move(func));
    }

    /**
     * @brief 从可调用对象订阅指定消息类型
     * @tparam Message 消息类型
     * @tparam F 可调用对象类型
     */
    template <typename Message, typename F>
    ScopedConnection subscribe(F&& func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        return subscribe<Message>(std::function<void(const Message&)>(std::forward<F>(func)));
    }

    /**
     * @brief 同步发布消息
     * @tparam Message 消息类型
     * @param message 消息实例
     */
    template <typename Message>
    void emit(const Message& message) const {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        if (auto* ch = findChannel<Message>()) {
            ch->m_event.emit(message);
        }
    }

    /**
     * @brief 异步投递消息
     * @tparam Message 消息类型
     * @param message 消息实例（可移动）
     */
    template <typename Message>
    void post(Message&& message) {
        using Decayed = std::decay_t<Message>;
        ensureChannel<Decayed>()->m_event.post(std::forward<Message>(message));
    }

    /// 派发所有通道的待处理消息
    void flush() {
        std::vector<IChannel*> ptrs;
        {
            detail::SharedLockGuard<LockPolicy> guard(m_lock);
            ptrs.reserve(m_channels.size());
            for (auto& [key, ch] : m_channels) {
                ptrs.push_back(ch.get());
            }
        }
        detail::forEachSafe(ptrs.begin(), ptrs.end(), [](auto* ch) { ch->flush(); });
    }

    /// 所有通道待派发消息总数
    [[nodiscard]] std::size_t pendingCount() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        std::size_t total = 0;
        for (const auto& [key, ch] : m_channels) {
            total += ch->pendingCount();
        }
        return total;
    }

private:
    /// 消息通道抽象基类
    struct IChannel {
        IChannel() = default;
        virtual ~IChannel() = default;
        IChannel(const IChannel&) = delete;
        IChannel& operator=(const IChannel&) = delete;
        virtual void flush() = 0;
        virtual std::size_t pendingCount() const = 0;
    };

    /**
     * @brief 类型化消息通道
     * @tparam Message 消息类型
     */
    template <typename Message>
    struct TypedChannel : IChannel {
        Event<void(const Message&), CollectAll, LockPolicy> m_event;  ///< 底层 void 事件
        void flush() override {
            m_event.flush();
        }
        std::size_t pendingCount() const override {
            return m_event.pendingCount();
        }
    };

    /// 获取或创建指定消息类型的通道
    template <typename Message>
    TypedChannel<Message>* ensureChannel() {
        detail::UniqueLockGuard<LockPolicy> guard(m_lock);
        const auto key = std::type_index(typeid(TypedChannel<Message>));
        auto it = m_channels.find(key);
        if (it == m_channels.end()) {
            it = m_channels.emplace(key, std::make_unique<TypedChannel<Message>>()).first;
        }
        return static_cast<TypedChannel<Message>*>(it->second.get());
    }

    /// 查找指定消息类型的通道（不存在返回 nullptr）
    template <typename Message>
    TypedChannel<Message>* findChannel() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        auto it = m_channels.find(std::type_index(typeid(TypedChannel<Message>)));
        return it != m_channels.end() ? static_cast<TypedChannel<Message>*>(it->second.get())
                                      : nullptr;
    }

    mutable LockPolicy m_lock;  ///< 锁策略
    std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_channels;  ///< 消息通道表
};

}  // namespace Dss::Evt

#ifdef DSS_EVT_RESTORE_EMIT
#undef DSS_EVT_RESTORE_EMIT
#define emit
#endif
