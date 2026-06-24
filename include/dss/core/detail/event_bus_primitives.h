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
    /// @brief 空实现的独占加锁操作。
    void lock() const noexcept {}
    /// @brief 空实现的独占解锁操作。
    void unlock() const noexcept {}
    /// @brief 空实现的共享加锁操作。
    void lock_shared() const noexcept {}
    /// @brief 空实现的共享解锁操作。
    void unlock_shared() const noexcept {}
};

/// 共享互斥锁策略（读写锁）
struct SharedMutexLock {
    mutable std::shared_mutex m_mutex;  ///< 底层读写锁

    /// @brief 获取独占锁。
    void lock() const {
        m_mutex.lock();
    }
    /// @brief 释放独占锁。
    void unlock() const {
        m_mutex.unlock();
    }
    /// @brief 获取共享锁。
    void lock_shared() const {
        m_mutex.lock_shared();
    }
    /// @brief 释放共享锁。
    void unlock_shared() const {
        m_mutex.unlock_shared();
    }
};

namespace detail {

/// 独占锁 RAII 守卫
template <typename L>
struct UniqueLockGuard {
    const L& lk;  ///< 锁策略引用

    /**
     * @brief 获取给定锁策略的独占锁。
     * @param l 需要加锁的锁策略。
     */
    explicit UniqueLockGuard(const L& l) : lk(l) {
        lk.lock();
    }

    /// @brief 释放构造时获取的独占锁。
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

    /**
     * @brief 获取给定锁策略的共享锁。
     * @param l 需要加锁的锁策略。
     */
    explicit SharedLockGuard(const L& l) : lk(l) {
        lk.lock_shared();
    }

    /// @brief 释放构造时获取的共享锁。
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

    /// @brief 默认移动构造可移动策略。
    MovePolicy(MovePolicy&&) noexcept = default;
    /**
     * @brief 默认移动赋值可移动策略。
     * @return 当前策略对象。
     */
    MovePolicy& operator=(MovePolicy&&) noexcept = default;
};

/// @brief 禁止移动构造和移动赋值的策略特化。
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
 * @param begin 处理器范围起始迭代器。
 * @param end 处理器范围结束迭代器。
 * @param fn 对每个处理器执行的回调。
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

/// @brief 复用组合器自定义累加器的适配器特化。
template <typename Combiner, typename R>
struct CombinerAdapter<Combiner, R, true> {
    using Accumulator = typename Combiner::Accumulator;  ///< 组合器提供的累加器类型。
};

/// @brief 为未提供累加器的组合器生成默认收集实现。
template <typename Combiner, typename R>
struct CombinerAdapter<Combiner, R, false> {
    /// @brief 收集所有返回值并在结束时调用组合器。
    struct Accumulator {
        std::vector<R> results;  ///< 收集的返回值列表

        /**
         * @brief 预留结果容量。
         * @param n 预计的处理器数量。
         */
        void reserve(std::size_t n) {
            results.reserve(n);
        }

        /**
         * @brief 追加一个处理器返回值。
         * @param val 待追加的返回值。
         */
        void add(R&& val) {
            results.push_back(std::move(val));
        }

        /**
         * @brief 查询是否应提前停止遍历。
         * @return 默认累加器始终返回 false。
         */
        [[nodiscard]] bool shouldStop() const {
            return false;
        }

        /**
         * @brief 组合已收集的返回值。
         * @return 组合器生成的最终结果。
         */
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

/// @brief 调用组合器自定义空结果函数的适配器特化。
template <typename Combiner>
struct EmptyResultAdapter<Combiner, true> {
    /**
     * @brief 获取组合器定义的空结果。
     * @return 组合器的空结果值。
     */
    static typename Combiner::result_type get() {
        return Combiner::emptyResult();
    }
};

/// @brief 为不支持空结果的组合器提供失败实现。
template <typename Combiner>
struct EmptyResultAdapter<Combiner, false> {
    /**
     * @brief 报告无处理器且组合器不支持空结果。
     * @return 此函数不会返回。
     * @throws std::logic_error 组合器要求至少一个处理器时抛出。
     */
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

    /**
     * @brief 移动构造连接并转移断开责任。
     * @param other 待转移的连接。
     */
    ScopedConnection(ScopedConnection&& other) noexcept {
        if (other.m_active.exchange(false, std::memory_order_acq_rel)) {
            m_disconnectFn = std::move(other.m_disconnectFn);
            m_active.store(true, std::memory_order_release);
        }
    }

    /**
     * @brief 移动赋值连接并先断开当前订阅。
     * @param other 待转移的连接。
     * @return 当前连接对象。
     */
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

    /// @brief 自动断开仍处于活跃状态的订阅。
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

    /**
     * @brief 查询连接是否仍处于活跃状态。
     * @return 尚未断开时返回 true。
     */
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

/**
 * @brief 类型擦除委托的函数签名特化。
 * @tparam R 返回值类型。
 * @tparam Args 参数类型列表。
 */
template <typename R, typename... Args>
class Delegate<R(Args...)> {
public:
    Delegate() = default;

    /**
     * @brief 使用标准函数包装器构造委托。
     * @param func 待绑定的可调用对象。
     */
    explicit Delegate(std::function<R(Args...)> func) : m_fn(std::move(func)) {}

    /**
     * @brief 从可调用对象构造委托。
     * @tparam F 可调用对象类型。
     * @param func 待绑定的可调用对象。
     * @return 绑定后的委托。
     */
    template <typename F>
    static Delegate from(F&& func) {
        return Delegate(std::function<R(Args...)>(std::forward<F>(func)));
    }

    /**
     * @brief 从函数指针构造委托。
     * @param func 待绑定的函数指针。
     * @return 绑定后的委托。
     */
    static Delegate from(R (*func)(Args...)) {
        return Delegate(std::function<R(Args...)>(func));
    }

    /**
     * @brief 从非 const 成员函数构造委托。
     * @tparam T 对象类型。
     * @param obj 成员函数所属对象的非拥有指针。
     * @param method 待绑定的成员函数。
     * @return 绑定后的委托。
     */
    template <typename T>
    static Delegate from(T* obj, R (T::*method)(Args...)) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    /**
     * @brief 从 const 成员函数构造委托。
     * @tparam T 对象类型。
     * @param obj 成员函数所属对象的非拥有指针。
     * @param method 待绑定的 const 成员函数。
     * @return 绑定后的委托。
     */
    template <typename T>
    static Delegate from(const T* obj, R (T::*method)(Args...) const) {
        return Delegate([obj, method](Args... args) -> R {
            return (obj->*method)(std::forward<Args>(args)...);
        });
    }

    /**
     * @brief 调用已绑定的可调用对象。
     * @param args 转发给可调用对象的参数。
     * @return 可调用对象的返回值。
     */
    R operator()(Args... args) const {
        assert(m_fn && "Delegate::operator() called on empty delegate");
        return m_fn(std::forward<Args>(args)...);
    }

    /**
     * @brief 查询委托是否已绑定可调用对象。
     * @return 已绑定时返回 true。
     */
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

    /**
     * @brief 构造没有处理器时的空结果。
     * @return 空返回值列表。
     */
    static result_type emptyResult() {
        return {};
    }

    /**
     * @brief 返回收集到的全部处理器结果。
     * @param results 已收集的返回值列表。
     * @return 合并后的返回值列表。
     */
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

    /// @brief 保存最近一次处理器返回值的增量累加器。
    struct Accumulator {
        std::optional<R> lastValue;  ///< 最近一次返回值

        /**
         * @brief 接收预计处理器数量。
         * @param count 处理器数量；该累加器无需实际分配。
         */
        void reserve(std::size_t count) {
            static_cast<void>(count);
        }

        /**
         * @brief 更新最近一次返回值。
         * @param val 新的处理器返回值。
         */
        void add(R&& val) {
            lastValue.emplace(std::move(val));
        }

        /**
         * @brief 查询是否应提前停止遍历。
         * @return 始终返回 false。
         */
        [[nodiscard]] bool shouldStop() const {
            return false;
        }

        /**
         * @brief 取得最后一个处理器返回值。
         * @return 最近一次返回值。
         */
        R finalize() {
            assert(lastValue.has_value());
            return std::move(*lastValue);
        }
    };

    /**
     * @brief 从结果列表中取得最后一个返回值。
     * @param results 非空返回值列表。
     * @return 列表末尾的返回值。
     */
    static result_type combine(std::vector<R>&& results) {
        assert(!results.empty());
        return std::move(results.back());
    }
};

}  // namespace Dss::Evt
