#include "dss/acquisition/frame_source_coordinator.h"

#include <utility>

namespace Dss::Acquisition {

FrameSourceCoordinator::~FrameSourceCoordinator() {
    stop();
}

auto FrameSourceCoordinator::registerSource(FrameSourceMode mode,
                                            std::shared_ptr<IFrameSource> source)
    -> std::expected<void, std::string> {
    if (!source) {
        return std::unexpected("frame source must not be null");
    }

    std::lock_guard lock(m_mutex);
    if (m_sources.contains(mode)) {
        return std::unexpected("frame source mode is already registered");
    }
    if (m_callback) {
        source->setFrameCallback(m_callback);
    }
    m_sources.emplace(mode, std::move(source));
    if (!m_activeMode.has_value()) {
        m_activeMode = mode;
    }
    return {};
}

auto FrameSourceCoordinator::selectSource(FrameSourceMode mode)
    -> std::expected<void, std::string> {
    std::lock_guard lock(m_mutex);
    const auto targetEntry = m_sources.find(mode);
    if (targetEntry == m_sources.end()) {
        return std::unexpected("requested frame source is not registered");
    }
    if (m_activeMode == mode) {
        return {};
    }

    const auto target = targetEntry->second;
    const auto initialized = target->init();
    if (!initialized.has_value()) {
        return initialized;
    }

    const auto current = activeSourceLocked();
    const auto resume = current && current->isRunning();
    if (current) {
        current->stop();
    }
    for (const auto& [registeredMode, source] : m_sources) {
        if (registeredMode != mode && source->isRunning()) {
            source->stop();
        }
    }

    m_activeMode = mode;
    if (resume) {
        target->start();
    }
    return {};
}

auto FrameSourceCoordinator::activeMode() const -> std::optional<FrameSourceMode> {
    std::lock_guard lock(m_mutex);
    return m_activeMode;
}

auto FrameSourceCoordinator::init() -> std::expected<void, std::string> {
    std::lock_guard lock(m_mutex);
    const auto source = activeSourceLocked();
    if (!source) {
        return std::unexpected("no active frame source");
    }
    return source->init();
}

void FrameSourceCoordinator::start() {
    std::lock_guard lock(m_mutex);
    const auto active = activeSourceLocked();
    if (!active) {
        return;
    }
    for (const auto& [mode, source] : m_sources) {
        if (m_activeMode != mode && source->isRunning()) {
            source->stop();
        }
    }
    active->start();
}

void FrameSourceCoordinator::stop() {
    std::lock_guard lock(m_mutex);
    for (const auto& [mode, source] : m_sources) {
        static_cast<void>(mode);
        if (source->isRunning()) {
            source->stop();
        }
    }
}

void FrameSourceCoordinator::setFrameCallback(FrameCallback callback) {
    std::lock_guard lock(m_mutex);
    m_callback = std::move(callback);
    for (const auto& [mode, source] : m_sources) {
        static_cast<void>(mode);
        source->setFrameCallback(m_callback);
    }
}

bool FrameSourceCoordinator::isRunning() const {
    std::lock_guard lock(m_mutex);
    const auto source = activeSourceLocked();
    return source && source->isRunning();
}

auto FrameSourceCoordinator::frameWidth() const -> uint32_t {
    std::lock_guard lock(m_mutex);
    const auto source = activeSourceLocked();
    return source ? source->frameWidth() : 0U;
}

auto FrameSourceCoordinator::frameHeight() const -> uint32_t {
    std::lock_guard lock(m_mutex);
    const auto source = activeSourceLocked();
    return source ? source->frameHeight() : 0U;
}

auto FrameSourceCoordinator::activeSourceLocked() const -> std::shared_ptr<IFrameSource> {
    if (!m_activeMode.has_value()) {
        return {};
    }
    const auto entry = m_sources.find(*m_activeMode);
    return entry == m_sources.end() ? std::shared_ptr<IFrameSource>{} : entry->second;
}

}  // namespace Dss::Acquisition
