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

#include "dss/core/detail/event_bus_primitives.h"
#include "dss/core/detail/event.h"

namespace Dss::Evt {
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

    mutable LockPolicy m_lock;                                                  ///< 锁策略
    std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_channels;  ///< 消息通道表
};

}  // namespace Dss::Evt

#ifdef DSS_EVT_RESTORE_EMIT
#undef DSS_EVT_RESTORE_EMIT
#define emit
#endif
