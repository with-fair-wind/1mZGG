#pragma once

#include <format>
#include <mutex>
#include <string>
#include <string_view>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"

namespace Dss::Core
{

class Logger
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    static auto instance() -> Logger&
    {
        static Logger logger;
        return logger;
    }

    void setBus(MessageBus* bus)
    {
        std::lock_guard lock(m_mutex);
        m_bus = bus;
    }

    void info(std::string_view msg)
    {
        emitLog(std::string(msg));
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args)
    {
        emitLog(std::format(fmt, std::forward<Args>(args)...));
    }

    void warn(std::string_view msg)
    {
        emitLog(std::format("[WARN] {}", msg));
    }

    void error(std::string_view msg)
    {
        emitLog(std::format("[ERROR] {}", msg));
    }

private:
    Logger() = default;

    void emitLog(std::string message)
    {
        MessageBus* bus = nullptr;
        {
            std::lock_guard lock(m_mutex);
            bus = m_bus;
        }

        if (bus)
        {
            bus->emit(LogMessageEvent{std::move(message)});
        }
    }

    MessageBus* m_bus = nullptr;
    std::mutex m_mutex;
};

} // namespace Dss::Core
