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

class ServiceRegistry {
public:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;

    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;

    template <typename Interface>
    void registerService(std::shared_ptr<Interface> impl) {
        registerService<Interface>("", std::move(impl));
    }

    template <typename Interface>
    void registerService(std::string_view name, std::shared_ptr<Interface> impl) {
        std::unique_lock lock(m_mutex);
        auto key = makeKey<Interface>(name);
        m_services[key] = std::move(impl);
    }

    template <typename Interface>
    [[nodiscard]] auto get() -> std::shared_ptr<Interface> {
        return get<Interface>("");
    }

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

    template <typename Interface>
    [[nodiscard]] auto tryGet() -> std::shared_ptr<Interface> {
        return tryGet<Interface>("");
    }

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

    template <typename Interface>
    [[nodiscard]] bool has() const {
        return has<Interface>("");
    }

    template <typename Interface>
    [[nodiscard]] bool has(std::string_view name) const {
        std::shared_lock lock(m_mutex);
        return m_services.contains(makeKey<Interface>(name));
    }

    void clear() {
        std::unique_lock lock(m_mutex);
        m_services.clear();
    }

private:
    struct ServiceKey {
        std::type_index type;
        std::string name;

        [[nodiscard]] bool operator==(const ServiceKey&) const = default;
    };

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

    mutable std::shared_mutex m_mutex;
    std::unordered_map<ServiceKey, std::any, ServiceKeyHash> m_services;
};

}  // namespace Dss::Core
