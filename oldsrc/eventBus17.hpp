#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace evt {

// ============================================================
// Lock Policies — 策略模式统一线程安全/非安全版本
// ============================================================

/// 无锁策略: 零开销, 用于单线程场景.
struct NoLock {
    void lock() const noexcept {}
    void unlock() const noexcept {}
    void lock_shared() const noexcept {}
    void unlock_shared() const noexcept {}
};

/// shared_mutex 策略: 支持读写分离, 用于多线程场景.
struct SharedMutexLock {
    mutable std::shared_mutex m_mutex;
    void lock() const { m_mutex.lock(); }
    void unlock() const { m_mutex.unlock(); }
    void lock_shared() const { m_mutex.lock_shared(); }
    void unlock_shared() const { m_mutex.unlock_shared(); }
};

namespace detail {

// ---- RAII 锁守卫 ----

template <typename L>
struct UniqueLockGuard {
    const L& lk;
    explicit UniqueLockGuard(const L& l) : lk(l) { lk.lock(); }
    ~UniqueLockGuard() { lk.unlock(); }
    UniqueLockGuard(const UniqueLockGuard&) = delete;
    UniqueLockGuard& operator=(const UniqueLockGuard&) = delete;
};

template <typename L>
struct SharedLockGuard {
    const L& lk;
    explicit SharedLockGuard(const L& l) : lk(l) { lk.lock_shared(); }
    ~SharedLockGuard() { lk.unlock_shared(); }
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
};

// ---- 条件删除 move ----

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

// ---- 异常安全遍历 ----

/// 对 [begin, end) 中每个元素调用 fn, 捕获并保留第一个异常, 遍历完成后重新抛出.
template <typename Iter, typename Fn>
void for_each_safe(Iter begin, Iter end, Fn&& fn) {
    std::exception_ptr first_ex;
    for (auto it = begin; it != end; ++it) {
        try {
            fn(*it);
        } catch (...) {
            if (!first_ex) {
                first_ex = std::current_exception();
            }
        }
    }
    if (first_ex) {
        std::rethrow_exception(first_ex);
    }
}

// ---- Combiner Accumulator 适配 ----

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
        std::vector<R> results;
        void reserve(std::size_t n) { results.reserve(n); }
        void add(R&& val) { results.push_back(std::move(val)); }
        bool should_stop() const { return false; }
        typename Combiner::result_type finalize() { return Combiner::combine(std::move(results)); }
    };
};

// ---- Combiner empty_result 适配 ----

template <typename C, typename = void>
struct HasEmptyResult : std::false_type {};

template <typename C>
struct HasEmptyResult<C, std::void_t<decltype(C::empty_result())>> : std::true_type {};

template <typename Combiner, bool Has>
struct EmptyResultAdapter;

template <typename Combiner>
struct EmptyResultAdapter<Combiner, true> {
    static typename Combiner::result_type get() { return Combiner::empty_result(); }
};

template <typename Combiner>
struct EmptyResultAdapter<Combiner, false> {
    [[noreturn]] static typename Combiner::result_type get() {
        throw std::logic_error("evt::Event::emit() called with no handlers on a Combiner that requires at least one");
    }
};

}  // namespace detail

// ============================================================
// ScopedConnection — 使用 atomic<bool> 保证 disconnect 仅执行一次
// ============================================================

class ScopedConnection {
   public:
    ScopedConnection() = default;

    explicit ScopedConnection(std::function<void()> disconnect_fn) : m_disconnect_fn(std::move(disconnect_fn)), m_active(true) {}

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ScopedConnection(ScopedConnection&& other) noexcept {
        if (other.m_active.exchange(false, std::memory_order_acq_rel)) {
            m_disconnect_fn = std::move(other.m_disconnect_fn);
            m_active.store(true, std::memory_order_release);
        }
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnect_noexcept();
            if (other.m_active.exchange(false, std::memory_order_acq_rel)) {
                m_disconnect_fn = std::move(other.m_disconnect_fn);
                m_active.store(true, std::memory_order_release);
            } else {
                m_disconnect_fn = nullptr;
            }
        }
        return *this;
    }

    ~ScopedConnection() noexcept { disconnect_noexcept(); }

    void disconnect() {
        if (m_active.exchange(false, std::memory_order_acq_rel)) {
            if (m_disconnect_fn) {
                auto fn = std::move(m_disconnect_fn);
                fn();
            }
        }
    }

    bool is_active() const { return m_active.load(std::memory_order_acquire); }

   private:
    void disconnect_noexcept() noexcept {
        try {
            disconnect();
        } catch (...) {  // NOLINT(bugprone-empty-catch)
        }
    }

    std::function<void()> m_disconnect_fn;
    std::atomic<bool> m_active{false};
};

// ============================================================
// Delegate — 类型安全的可调用对象包装
// ============================================================

template <typename Signature>
class Delegate;

template <typename R, typename... Args>
class Delegate<R(Args...)> {
   public:
    Delegate() = default;

    explicit Delegate(std::function<R(Args...)> func) : m_fn(std::move(func)) {}

    template <typename F>
    static Delegate from(F&& func) {
        return Delegate(std::function<R(Args...)>(std::forward<F>(func)));
    }

    static Delegate from(R (*func)(Args...)) { return Delegate(std::function<R(Args...)>(func)); }

    template <typename T>
    static Delegate from(T* obj, R (T::*method)(Args...)) {
        return Delegate([obj, method](Args... args) -> R { return (obj->*method)(std::forward<Args>(args)...); });
    }

    template <typename T>
    static Delegate from(const T* obj, R (T::*method)(Args...) const) {
        return Delegate([obj, method](Args... args) -> R { return (obj->*method)(std::forward<Args>(args)...); });
    }

    R operator()(Args... args) const {
        assert(m_fn && "Delegate::operator() called on empty delegate");
        return m_fn(std::forward<Args>(args)...);
    }

    bool valid() const { return static_cast<bool>(m_fn); }

   private:
    std::function<R(Args...)> m_fn;
};

// ============================================================
// Combiners — 合并策略
// ============================================================

/// 默认: 收集所有返回值到 std::vector.
template <typename R>
struct CollectAll {
    using result_type = std::vector<R>;
    static result_type empty_result() { return {}; }
    static result_type combine(std::vector<R>&& results) { return std::move(results); }
};

/// 返回最后一个 handler 的值.
template <typename R>
struct LastValue {
    using result_type = R;
    struct Accumulator {
        std::optional<R> last_value;
        void reserve(std::size_t) {}
        void add(R&& val) { last_value.emplace(std::move(val)); }
        bool should_stop() const { return false; }
        R finalize() {
            assert(last_value.has_value() && "LastValue combiner requires at least one handler");
            return std::move(*last_value);
        }
    };
    static result_type combine(std::vector<R>&& results) {  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        assert(!results.empty() && "LastValue combiner requires at least one handler");
        return std::move(results.back());
    }
};

/// 短路求值: 遇到 true 立即停止.
template <typename R>
struct StopOnTrue {
    using result_type = R;
    struct Accumulator {
        std::optional<R> last_value;
        bool stopped = false;
        void reserve(std::size_t) {}
        void add(R&& val) {
            last_value.emplace(std::move(val));
            if (*last_value) {
                stopped = true;
            }
        }
        bool should_stop() const { return stopped; }
        R finalize() {
            assert(last_value.has_value() && "StopOnTrue combiner requires at least one handler");
            return std::move(*last_value);
        }
    };
};

/// 短路求值: 遇到 false 立即停止.
template <typename R>
struct StopOnFalse {
    using result_type = R;
    struct Accumulator {
        std::optional<R> last_value;
        bool stopped = false;
        void reserve(std::size_t) {}
        void add(R&& val) {
            last_value.emplace(std::move(val));
            if (!*last_value) {
                stopped = true;
            }
        }
        bool should_stop() const { return stopped; }
        R finalize() {
            assert(last_value.has_value() && "StopOnFalse combiner requires at least one handler");
            return std::move(*last_value);
        }
    };
};

// ============================================================
// EventCore — COW 快照 + 锁策略 (Event 的内部共享状态)
// ============================================================

namespace detail {

template <typename Handler, typename LockPolicy, typename... Args>
class EventCore {
   public:
    using Id = std::size_t;

    struct Slot {
        Id id;
        Handler handler;
    };

    using HandlerList = std::vector<Slot>;
    using StoredArgs = std::tuple<std::decay_t<Args>...>;

    EventCore() : m_handlers(std::make_shared<HandlerList>()) {}

    /// COW 快照: O(1), 仅增加 shared_ptr 引用计数.
    std::shared_ptr<HandlerList> snapshot() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers;
    }

    Id add_handler(Handler handler) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cow_detach();
        Id id = m_next_id++;
        m_handlers->push_back(Slot{id, std::move(handler)});
        return id;
    }

    void remove_handler(Id id) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cow_detach();
        auto& items = *m_handlers;
        items.erase(std::remove_if(items.begin(), items.end(), [id](const Slot& s) { return s.id == id; }), items.end());
    }

    void clear_handlers() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_handlers = std::make_shared<HandlerList>();
    }

    std::size_t handler_count() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers ? m_handlers->size() : 0;
    }

    template <typename... UArgs>
    void push_pending(UArgs&&... uargs) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.emplace_back(std::decay_t<Args>(std::forward<UArgs>(uargs))...);
    }

    std::vector<StoredArgs> take_pending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        return std::move(m_pending);
    }

    std::size_t pending_count() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_pending.size();
    }

    void clear_pending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.clear();
    }

   private:
    void cow_detach() {
        if (!m_handlers) {
            m_handlers = std::make_shared<HandlerList>();
        } else if (m_handlers.use_count() > 1) {
            m_handlers = std::make_shared<HandlerList>(*m_handlers);
        }
    }

    mutable LockPolicy m_lock;
    std::shared_ptr<HandlerList> m_handlers;
    Id m_next_id = 1;
    std::vector<StoredArgs> m_pending;
};

}  // namespace detail

// ============================================================
// Event — 主模板声明
// ============================================================

template <typename Signature, template <typename> class CombinerT = CollectAll, typename LockPolicy = NoLock>
class Event;

// ============================================================
// Event 特化: void 返回值
// ============================================================

template <typename... Args, template <typename> class CombinerT, typename LockPolicy>
class Event<void(Args...), CombinerT, LockPolicy> : private detail::MovePolicy<true> {
   public:
    using Handler = Delegate<void(Args...)>;
    using Id = std::size_t;

   private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;
    using StoredArgs = typename Core::StoredArgs;

   public:
    Event() : m_core(std::make_shared<Core>()) {}

    ScopedConnection subscribe(Handler handler) {
        ensure_core();
        Id id = m_core->add_handler(std::move(handler));
        std::weak_ptr<Core> weak = m_core;
        return ScopedConnection([weak, id]() {
            if (auto core = weak.lock()) {
                core->remove_handler(id);
            }
        });
    }

    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    void clear() {
        if (m_core) {
            m_core->clear_handlers();
        }
    }

    std::size_t size() const { return m_core ? m_core->handler_count() : 0; }

    bool empty() const { return size() == 0; }

    /// 触发事件: COW 快照 O(1), 遍历时不受 subscribe/disconnect 影响.
    void emit(Args... args) const {
        if (!m_core) {
            return;
        }
        auto snap = m_core->snapshot();
        if (!snap || snap->empty()) {
            return;
        }
        detail::for_each_safe(snap->begin(), snap->end(), [&](const auto& slot) {
            if (slot.handler.valid()) {
                slot.handler(static_cast<Args>(args)...);
            }
        });
    }

    void operator()(Args... args) const { emit(std::forward<Args>(args)...); }

    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensure_core();
        m_core->push_pending(std::forward<UArgs>(uargs)...);
    }

    /// 派发所有挂起的事件. flush 过程中新 post 的事件不会在本次 flush 中派发.
    void flush() {
        if (!m_core) {
            return;
        }
        auto batch = m_core->take_pending();
        detail::for_each_safe(batch.begin(), batch.end(), [this](auto& stored) { std::apply([this](auto&... args) { emit(static_cast<Args>(args)...); }, stored); });
    }

    std::size_t pending_count() const { return m_core ? m_core->pending_count() : 0; }

    void clear_pending() {
        if (m_core) {
            m_core->clear_pending();
        }
    }

   private:
    void ensure_core() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    std::shared_ptr<Core> m_core;
};

// ============================================================
// Event 特化: 非 void 返回值 — 支持 Combiner + 短路求值
// ============================================================

template <typename R, typename... Args, template <typename> class CombinerT, typename LockPolicy>
class Event<R(Args...), CombinerT, LockPolicy> : private detail::MovePolicy<true> {
   public:
    using Handler = Delegate<R(Args...)>;
    using Id = std::size_t;
    using Combiner = CombinerT<R>;
    using ResultType = typename Combiner::result_type;

   private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;
    using StoredArgs = typename Core::StoredArgs;
    using Adapter = detail::CombinerAdapter<Combiner, R, detail::HasAccumulator<Combiner, R>::value>;

   public:
    Event() : m_core(std::make_shared<Core>()) {}

    ScopedConnection subscribe(Handler handler) {
        ensure_core();
        Id id = m_core->add_handler(std::move(handler));
        std::weak_ptr<Core> weak = m_core;
        return ScopedConnection([weak, id]() {
            if (auto core = weak.lock()) {
                core->remove_handler(id);
            }
        });
    }

    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    void clear() {
        if (m_core) {
            m_core->clear_handlers();
        }
    }

    std::size_t size() const { return m_core ? m_core->handler_count() : 0; }

    bool empty() const { return size() == 0; }

    /// 触发事件: 调用所有 handler, 通过 Combiner 合并返回值.
    /// 支持短路求值: 如果 Combiner 提供 Accumulator, 可提前终止遍历.
    ResultType emit(Args... args) const {
        if (!m_core) {
            return empty_result();
        }
        auto snap = m_core->snapshot();
        if (!snap || snap->empty()) {
            return empty_result();
        }
        typename Adapter::Accumulator acc;
        acc.reserve(snap->size());
        std::exception_ptr first_ex;
        for (const auto& slot : *snap) {
            if (slot.handler.valid()) {
                try {
                    acc.add(slot.handler(static_cast<Args>(args)...));
                    if (acc.should_stop()) {
                        break;
                    }
                } catch (...) {
                    if (!first_ex) {
                        first_ex = std::current_exception();
                    }
                }
            }
        }
        if (first_ex) {
            std::rethrow_exception(first_ex);
        }
        return acc.finalize();
    }

    ResultType operator()(Args... args) const { return emit(std::forward<Args>(args)...); }

    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensure_core();
        m_core->push_pending(std::forward<UArgs>(uargs)...);
    }

    /// 派发所有挂起的事件, 每个批次产生一个合并结果.
    std::vector<ResultType> flush() {
        if (!m_core) {
            return {};
        }
        auto batch = m_core->take_pending();
        std::vector<ResultType> all_results;
        all_results.reserve(batch.size());
        detail::for_each_safe(batch.begin(), batch.end(), [&](auto& stored) { all_results.push_back(std::apply([this](auto&... args) { return emit(static_cast<Args>(args)...); }, stored)); });
        return all_results;
    }

    std::size_t pending_count() const { return m_core ? m_core->pending_count() : 0; }

    void clear_pending() {
        if (m_core) {
            m_core->clear_pending();
        }
    }

   private:
    void ensure_core() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    static ResultType empty_result() {
        return detail::EmptyResultAdapter<Combiner, detail::HasEmptyResult<Combiner>::value>::get();
    }

    std::shared_ptr<Core> m_core;
};

// ============================================================
// BasicMessageBus — 基于类型的消息总线 (策略模式支持线程安全)
// ============================================================

template <typename LockPolicy = NoLock>
class BasicMessageBus : private detail::MovePolicy<std::is_same_v<LockPolicy, NoLock>> {
   public:
    BasicMessageBus() = default;

    // ---- void 返回 ----

    template <typename Message>
    ScopedConnection subscribe(std::function<void(const Message&)> func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        return ensure_channel<Message>()->m_event.subscribe(std::move(func));
    }

    template <typename Message, typename F>
    ScopedConnection subscribe(F&& func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        return subscribe<Message>(std::function<void(const Message&)>(std::forward<F>(func)));
    }

    template <typename Message>
    void emit(const Message& message) const {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_channel<Message>()) {
            ch->m_event.emit(message);
        }
    }

    template <typename Message>
    void clear() {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_channel<Message>()) {
            ch->m_event.clear();
        }
    }

    // ---- 非 void 返回 ----

    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    ScopedConnection subscribe(std::function<R(const Message&)> func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        return ensure_ret_channel<Message, R, CombinerTC>()->m_event.subscribe(std::move(func));
    }

    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    typename CombinerTC<R>::result_type emit(const Message& message) const {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_ret_channel<Message, R, CombinerTC>()) {
            return ch->m_event.emit(message);
        }
        return detail::EmptyResultAdapter<CombinerTC<R>, detail::HasEmptyResult<CombinerTC<R>>::value>::get();
    }

    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    void clear() {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_ret_channel<Message, R, CombinerTC>()) {
            ch->m_event.clear();
        }
    }

    // ---- post / flush ----

    template <typename Message>
    void post(Message&& message) {
        using Decayed = std::decay_t<Message>;
        ensure_channel<Decayed>()->m_event.post(std::forward<Message>(message));
    }

    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    void post(const Message& message) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        ensure_ret_channel<Message, R, CombinerTC>()->m_event.post(message);
    }

    void flush() {
        std::vector<IChannel*> ptrs;
        {
            detail::SharedLockGuard<LockPolicy> guard(m_lock);
            ptrs.reserve(m_channels.size());
            for (auto& [key, ch] : m_channels) {
                ptrs.push_back(ch.get());
            }
        }
        detail::for_each_safe(ptrs.begin(), ptrs.end(), [](auto* ch) { ch->flush(); });
    }

    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    std::vector<typename CombinerTC<R>::result_type> flush() {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_ret_channel<Message, R, CombinerTC>()) {
            auto results = ch->m_event.flush();
            if (!results.empty()) {
                return results;
            }
            return ch->take_results();
        }
        return {};
    }

    /// 获取上次通用 flush() 时非 void 通道缓存的返回值 (move 语义, 仅取一次).
    template <typename Message, typename R, template <typename> class CombinerTC = CollectAll, std::enable_if_t<!std::is_void_v<R>, int> = 0>
    std::vector<typename CombinerTC<R>::result_type> take_flush_results() {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>, "Message type must not be cv-qualified or a reference.");
        if (auto* ch = find_ret_channel<Message, R, CombinerTC>()) {
            return ch->take_results();
        }
        return {};
    }

    std::size_t pending_count() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        std::size_t total = 0;
        for (const auto& [key, ch] : m_channels) {
            total += ch->pending_count();
        }
        return total;
    }

   private:
    struct IChannel {
        IChannel() = default;
        virtual ~IChannel() = default;
        IChannel(const IChannel&) = delete;
        IChannel& operator=(const IChannel&) = delete;

        virtual void flush() = 0;
        virtual std::size_t pending_count() const = 0;
    };

    template <typename Message>
    struct TypedChannel : IChannel {
        Event<void(const Message&), CollectAll, LockPolicy> m_event;
        void flush() override { m_event.flush(); }
        std::size_t pending_count() const override { return m_event.pending_count(); }
    };

    template <typename Message, typename R, template <typename> class CombinerTC>
    struct TypedRetChannel : IChannel {
        Event<R(const Message&), CombinerTC, LockPolicy> m_event;
        mutable LockPolicy m_result_lock;
        std::vector<typename CombinerTC<R>::result_type> m_last_flush_results;

        void flush() override {
            auto results = m_event.flush();
            detail::UniqueLockGuard<LockPolicy> guard(m_result_lock);
            m_last_flush_results = std::move(results);
        }

        std::vector<typename CombinerTC<R>::result_type> take_results() {
            detail::UniqueLockGuard<LockPolicy> guard(m_result_lock);
            return std::move(m_last_flush_results);
        }

        std::size_t pending_count() const override { return m_event.pending_count(); }
    };

    template <typename Message>
    TypedChannel<Message>* ensure_channel() {
        detail::UniqueLockGuard<LockPolicy> guard(m_lock);
        const auto key = std::type_index(typeid(TypedChannel<Message>));
        auto it = m_channels.find(key);
        if (it == m_channels.end()) {
            it = m_channels.emplace(key, std::make_unique<TypedChannel<Message>>()).first;
        }
        return static_cast<TypedChannel<Message>*>(it->second.get());
    }

    template <typename Message>
    TypedChannel<Message>* find_channel() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        auto it = m_channels.find(std::type_index(typeid(TypedChannel<Message>)));
        return it != m_channels.end() ? static_cast<TypedChannel<Message>*>(it->second.get()) : nullptr;
    }

    template <typename Message, typename R, template <typename> class CombinerTC>
    TypedRetChannel<Message, R, CombinerTC>* ensure_ret_channel() {
        detail::UniqueLockGuard<LockPolicy> guard(m_lock);
        const auto key = std::type_index(typeid(TypedRetChannel<Message, R, CombinerTC>));
        auto it = m_channels.find(key);
        if (it == m_channels.end()) {
            it = m_channels.emplace(key, std::make_unique<TypedRetChannel<Message, R, CombinerTC>>()).first;
        }
        return static_cast<TypedRetChannel<Message, R, CombinerTC>*>(it->second.get());
    }

    template <typename Message, typename R, template <typename> class CombinerTC>
    TypedRetChannel<Message, R, CombinerTC>* find_ret_channel() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        auto it = m_channels.find(std::type_index(typeid(TypedRetChannel<Message, R, CombinerTC>)));
        return it != m_channels.end() ? static_cast<TypedRetChannel<Message, R, CombinerTC>*>(it->second.get()) : nullptr;
    }

    mutable LockPolicy m_lock;
    std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_channels;
};

}  // namespace evt
