#pragma once

// C++23-ready event bus, based on the project's eventBus17.hpp.
// This header re-exports the Dss::Evt namespace for use within Dss.
// Future: upgrade SFINAE to concepts, std::function to std::move_only_function.

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

// Qt defines `emit` as a macro. We must suppress it here because
// Event::emit() is a legitimate method name in non-Qt event systems.
#ifdef emit
#undef emit
#define DSS_EVT_RESTORE_EMIT
#endif

namespace Dss::Evt {

// ============================================================
// Lock Policies
// ============================================================

struct NoLock {
    void lock() const noexcept {}
    void unlock() const noexcept {}
    void lock_shared() const noexcept {}
    void unlock_shared() const noexcept {}
};

struct SharedMutexLock {
    mutable std::shared_mutex m_mutex;
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

template <typename L>
struct UniqueLockGuard {
    const L& lk;
    explicit UniqueLockGuard(const L& l) : lk(l) {
        lk.lock();
    }
    ~UniqueLockGuard() {
        lk.unlock();
    }
    UniqueLockGuard(const UniqueLockGuard&) = delete;
    UniqueLockGuard& operator=(const UniqueLockGuard&) = delete;
};

template <typename L>
struct SharedLockGuard {
    const L& lk;
    explicit SharedLockGuard(const L& l) : lk(l) {
        lk.lock_shared();
    }
    ~SharedLockGuard() {
        lk.unlock_shared();
    }
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
};

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
        std::vector<R> results;
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

// ============================================================
// ScopedConnection
// ============================================================

class ScopedConnection {
public:
    ScopedConnection() = default;

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

    void disconnect() {
        if (m_active.exchange(false, std::memory_order_acq_rel)) {
            if (m_disconnectFn) {
                auto fn = std::move(m_disconnectFn);
                fn();
            }
        }
    }

    [[nodiscard]] bool isActive() const {
        return m_active.load(std::memory_order_acquire);
    }

private:
    void disconnectNoexcept() noexcept {
        try {
            disconnect();
        } catch (...) {
        }
    }

    std::function<void()> m_disconnectFn;
    std::atomic<bool> m_active{false};
};

// ============================================================
// Delegate
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

    static Delegate from(R (*func)(Args...)) {
        return Delegate(std::function<R(Args...)>(func));
    }

    template <typename T>
    static Delegate from(T* obj, R (T::*method)(Args...)) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    template <typename T>
    static Delegate from(const T* obj, R (T::*method)(Args...) const) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    R operator()(Args... args) const {
        assert(m_fn && "Delegate::operator() called on empty delegate");
        return m_fn(std::forward<Args>(args)...);
    }

    [[nodiscard]] bool valid() const {
        return static_cast<bool>(m_fn);
    }

private:
    std::function<R(Args...)> m_fn;
};

// ============================================================
// Combiners
// ============================================================

template <typename R>
struct CollectAll {
    using result_type = std::vector<R>;
    static result_type emptyResult() {
        return {};
    }
    static result_type combine(std::vector<R>&& results) {
        return std::move(results);
    }
};

template <typename R>
struct LastValue {
    using result_type = R;
    struct Accumulator {
        std::optional<R> lastValue;
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

// ============================================================
// EventCore — COW snapshot + lock policy
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

    std::shared_ptr<HandlerList> snapshot() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers;
    }

    Id addHandler(Handler handler) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cowDetach();
        Id id = m_nextId++;
        m_handlers->push_back(Slot{id, std::move(handler)});
        return id;
    }

    void removeHandler(Id id) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        cowDetach();
        auto& items = *m_handlers;
        std::erase_if(items, [id](const Slot& s) { return s.id == id; });
    }

    void clearHandlers() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_handlers = std::make_shared<HandlerList>();
    }

    [[nodiscard]] std::size_t handlerCount() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_handlers ? m_handlers->size() : 0;
    }

    template <typename... UArgs>
    void pushPending(UArgs&&... uargs) {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.emplace_back(std::decay_t<Args>(std::forward<UArgs>(uargs))...);
    }

    std::vector<StoredArgs> takePending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        return std::move(m_pending);
    }

    [[nodiscard]] std::size_t pendingCount() const {
        SharedLockGuard<LockPolicy> guard(m_lock);
        return m_pending.size();
    }

    void clearPending() {
        UniqueLockGuard<LockPolicy> guard(m_lock);
        m_pending.clear();
    }

private:
    void cowDetach() {
        if (!m_handlers) {
            m_handlers = std::make_shared<HandlerList>();
        } else if (m_handlers.use_count() > 1) {
            m_handlers = std::make_shared<HandlerList>(*m_handlers);
        }
    }

    mutable LockPolicy m_lock;
    std::shared_ptr<HandlerList> m_handlers;
    Id m_nextId = 1;
    std::vector<StoredArgs> m_pending;
};

}  // namespace detail

// ============================================================
// Event — void specialization
// ============================================================

template <typename Signature, template <typename> class CombinerT = CollectAll,
          typename LockPolicy = NoLock>
class Event;

template <typename... Args, template <typename> class CombinerT, typename LockPolicy>
class Event<void(Args...), CombinerT, LockPolicy> : private detail::MovePolicy<true> {
public:
    using Handler = Delegate<void(Args...)>;
    using Id = std::size_t;

private:
    using Core = detail::EventCore<Handler, LockPolicy, Args...>;

public:
    Event() : m_core(std::make_shared<Core>()) {}

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

    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    void clear() {
        if (m_core) {
            m_core->clearHandlers();
        }
    }

    [[nodiscard]] std::size_t size() const {
        return m_core ? m_core->handlerCount() : 0;
    }
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

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

    void operator()(Args... args) const {
        emit(std::forward<Args>(args)...);
    }

    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensureCore();
        m_core->pushPending(std::forward<UArgs>(uargs)...);
    }

    void flush() {
        if (!m_core) {
            return;
        }
        auto batch = m_core->takePending();
        detail::forEachSafe(batch.begin(), batch.end(), [this](auto& stored) {
            std::apply([this](auto&... args) { emit(static_cast<Args>(args)...); }, stored);
        });
    }

    [[nodiscard]] std::size_t pendingCount() const {
        return m_core ? m_core->pendingCount() : 0;
    }

    void clearPending() {
        if (m_core) {
            m_core->clearPending();
        }
    }

private:
    void ensureCore() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    std::shared_ptr<Core> m_core;
};

// ============================================================
// Event — non-void specialization with Combiner
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
    using Adapter =
        detail::CombinerAdapter<Combiner, R, detail::HasAccumulator<Combiner, R>::value>;

public:
    Event() : m_core(std::make_shared<Core>()) {}

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

    template <typename F>
    ScopedConnection subscribe(F&& func) {
        return subscribe(Handler::from(std::forward<F>(func)));
    }

    void clear() {
        if (m_core) {
            m_core->clearHandlers();
        }
    }

    [[nodiscard]] std::size_t size() const {
        return m_core ? m_core->handlerCount() : 0;
    }
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

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

    ResultType operator()(Args... args) const {
        return emit(std::forward<Args>(args)...);
    }

    template <typename... UArgs>
    void post(UArgs&&... uargs) {
        ensureCore();
        m_core->pushPending(std::forward<UArgs>(uargs)...);
    }

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

    [[nodiscard]] std::size_t pendingCount() const {
        return m_core ? m_core->pendingCount() : 0;
    }

    void clearPending() {
        if (m_core) {
            m_core->clearPending();
        }
    }

private:
    void ensureCore() {
        if (!m_core) {
            m_core = std::make_shared<Core>();
        }
    }

    static ResultType emptyResult() {
        return detail::EmptyResultAdapter<Combiner, detail::HasEmptyResult<Combiner>::value>::get();
    }

    std::shared_ptr<Core> m_core;
};

// ============================================================
// BasicMessageBus — type-based message bus
// ============================================================

template <typename LockPolicy = NoLock>
class BasicMessageBus : private detail::MovePolicy<std::is_same_v<LockPolicy, NoLock>> {
public:
    BasicMessageBus() = default;

    template <typename Message>
    ScopedConnection subscribe(std::function<void(const Message&)> func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        return ensureChannel<Message>()->m_event.subscribe(std::move(func));
    }

    template <typename Message, typename F>
    ScopedConnection subscribe(F&& func) {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        return subscribe<Message>(std::function<void(const Message&)>(std::forward<F>(func)));
    }

    template <typename Message>
    void emit(const Message& message) const {
        static_assert(std::is_same_v<Message, std::decay_t<Message>>);
        if (auto* ch = findChannel<Message>()) {
            ch->m_event.emit(message);
        }
    }

    template <typename Message>
    void post(Message&& message) {
        using Decayed = std::decay_t<Message>;
        ensureChannel<Decayed>()->m_event.post(std::forward<Message>(message));
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
        detail::forEachSafe(ptrs.begin(), ptrs.end(), [](auto* ch) { ch->flush(); });
    }

    [[nodiscard]] std::size_t pendingCount() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        std::size_t total = 0;
        for (const auto& [key, ch] : m_channels) {
            total += ch->pendingCount();
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
        virtual std::size_t pendingCount() const = 0;
    };

    template <typename Message>
    struct TypedChannel : IChannel {
        Event<void(const Message&), CollectAll, LockPolicy> m_event;
        void flush() override {
            m_event.flush();
        }
        std::size_t pendingCount() const override {
            return m_event.pendingCount();
        }
    };

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

    template <typename Message>
    TypedChannel<Message>* findChannel() const {
        detail::SharedLockGuard<LockPolicy> guard(m_lock);
        auto it = m_channels.find(std::type_index(typeid(TypedChannel<Message>)));
        return it != m_channels.end() ? static_cast<TypedChannel<Message>*>(it->second.get())
                                      : nullptr;
    }

    mutable LockPolicy m_lock;
    std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_channels;
};

}  // namespace Dss::Evt

#ifdef DSS_EVT_RESTORE_EMIT
#undef DSS_EVT_RESTORE_EMIT
#define emit
#endif
