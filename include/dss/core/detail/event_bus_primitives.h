#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

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

template <typename C, typename = void>
struct HasAccumulator : std::false_type {};

template <typename C>
struct HasAccumulator<C, std::void_t<typename C::Accumulator>> : std::true_type {};

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


}  // namespace Dss::Evt
