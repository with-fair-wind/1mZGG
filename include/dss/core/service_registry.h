#pragma once

#include <any>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace Dss::Core {

/// 服务注册表，按接口类型和名称管理共享服务实例
class ServiceRegistry {
public:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;

    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;

    /**
     * @brief 注册默认名称的服务实例
     * @tparam Interface 服务接口类型
     * @param impl 服务实现（共享所有权）
     */
    template <typename Interface>
    void registerService(std::shared_ptr<Interface> impl) {
        registerService<Interface>("", std::move(impl));
    }

    /**
     * @brief 注册指定名称的服务实例
     * @tparam Interface 服务接口类型
     * @param name 服务名称（空字符串表示默认实例）
     * @param impl 服务实现（共享所有权）
     */
    template <typename Interface>
    void registerService(std::string_view name, std::shared_ptr<Interface> impl) {
        std::unique_lock lock(m_mutex);
        auto key = makeKey<Interface>(name);
        m_services[key] = std::move(impl);
    }

    /**
     * @brief 获取默认名称的服务实例
     * @tparam Interface 服务接口类型
     * @return 服务共享指针
     * @throws std::runtime_error 服务未注册时
     */
    template <typename Interface>
    [[nodiscard]] auto get() -> std::shared_ptr<Interface> {
        return get<Interface>("");
    }

    /**
     * @brief 获取指定名称的服务实例
     * @tparam Interface 服务接口类型
     * @param name 服务名称
     * @return 服务共享指针
     * @throws std::runtime_error 服务未注册时
     */
    template <typename Interface>
    [[nodiscard]] auto get(std::string_view name) -> std::shared_ptr<Interface> {
        std::shared_lock lock(m_mutex);
        auto key = makeKey<Interface>(name);
        auto it = m_services.find(key);
        if (it == m_services.end()) {
            throw std::runtime_error(unregisteredMessage<Interface>(name));
        }
        return std::any_cast<std::shared_ptr<Interface>>(it->second);
    }

    /**
     * @brief 尝试获取默认名称的服务实例
     * @tparam Interface 服务接口类型
     * @return 服务共享指针，未注册时返回 nullptr
     */
    template <typename Interface>
    [[nodiscard]] auto tryGet() -> std::shared_ptr<Interface> {
        return tryGet<Interface>("");
    }

    /**
     * @brief 尝试获取指定名称的服务实例
     * @tparam Interface 服务接口类型
     * @param name 服务名称
     * @return 服务共享指针，未注册时返回 nullptr
     */
    template <typename Interface>
    [[nodiscard]] auto tryGet(std::string_view name) -> std::shared_ptr<Interface> {
        std::shared_lock lock(m_mutex);
        auto key = makeKey<Interface>(name);
        auto it = m_services.find(key);
        if (it == m_services.end()) {
            return nullptr;
        }
        return std::any_cast<std::shared_ptr<Interface>>(it->second);
    }

    /**
     * @brief 检查默认名称的服务是否已注册
     * @tparam Interface 服务接口类型
     */
    template <typename Interface>
    [[nodiscard]] bool has() const {
        return has<Interface>("");
    }

    /**
     * @brief 检查指定名称的服务是否已注册
     * @tparam Interface 服务接口类型
     * @param name 服务名称
     */
    template <typename Interface>
    [[nodiscard]] bool has(std::string_view name) const {
        std::shared_lock lock(m_mutex);
        return m_services.contains(makeKey<Interface>(name));
    }

    /// 清除所有已注册的服务
    void clear() {
        std::unique_lock lock(m_mutex);
        m_services.clear();
    }

private:
    /// 服务查找键（类型 + 名称）
    struct ServiceKey {
        std::type_index type;  ///< 接口类型索引
        std::string name;      ///< 服务名称

        [[nodiscard]] bool operator==(const ServiceKey&) const = default;
    };

    /// ServiceKey 哈希函数
    struct ServiceKeyHash {
        [[nodiscard]] auto operator()(const ServiceKey& key) const noexcept -> std::size_t {
            auto seed = key.type.hash_code();
            seed ^= std::hash<std::string>{}(key.name) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
            return seed;
        }
    };

    template <typename Interface>
    [[nodiscard]] static auto makeKey(std::string_view name) -> ServiceKey {
        return ServiceKey{std::type_index(typeid(Interface)), std::string(name)};
    }

    template <typename Interface>
    [[nodiscard]] static auto unregisteredMessage(std::string_view name) -> std::string {
        auto message = std::string("Service not registered: ") + typeid(Interface).name();
        if (!name.empty()) {
            message += " named ";
            message.append(name);
        }
        return message;
    }

    mutable std::shared_mutex m_mutex;  ///< 读写锁，保护服务表
    std::unordered_map<ServiceKey, std::any, ServiceKeyHash> m_services;  ///< 已注册服务表
};

}  // namespace Dss::Core
