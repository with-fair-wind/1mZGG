#pragma once

#include <any>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace Dss::Core
{

class ServiceRegistry
{
public:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;

    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;

    template <typename Interface>
    void registerService(std::shared_ptr<Interface> impl)
    {
        std::unique_lock lock(m_mutex);
        auto key = std::type_index(typeid(Interface));
        m_services[key] = std::move(impl);
    }

    template <typename Interface>
    [[nodiscard]] auto get() -> std::shared_ptr<Interface>
    {
        std::shared_lock lock(m_mutex);
        auto key = std::type_index(typeid(Interface));
        auto it = m_services.find(key);
        if (it == m_services.end())
        {
            throw std::runtime_error(
                std::string("Service not registered: ") + typeid(Interface).name());
        }
        return std::any_cast<std::shared_ptr<Interface>>(it->second);
    }

    template <typename Interface>
    [[nodiscard]] auto tryGet() -> std::shared_ptr<Interface>
    {
        std::shared_lock lock(m_mutex);
        auto key = std::type_index(typeid(Interface));
        auto it = m_services.find(key);
        if (it == m_services.end())
        {
            return nullptr;
        }
        return std::any_cast<std::shared_ptr<Interface>>(it->second);
    }

    template <typename Interface>
    [[nodiscard]] bool has() const
    {
        std::shared_lock lock(m_mutex);
        return m_services.contains(std::type_index(typeid(Interface)));
    }

    void clear()
    {
        std::unique_lock lock(m_mutex);
        m_services.clear();
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::type_index, std::any> m_services;
};

} // namespace Dss::Core
