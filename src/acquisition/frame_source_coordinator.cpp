#include "dss/acquisition/source/frame_source_coordinator.h"

#include <utility>
#include <vector>

namespace Dss::Acquisition {

FrameSourceCoordinator::~FrameSourceCoordinator() {
    stop();
}

FrameSourceCoordinator::LifecycleOperation::LifecycleOperation(FrameSourceCoordinator& owner)
    : m_owner(owner) {
    m_owner.beginLifecycleOperation();
}

FrameSourceCoordinator::LifecycleOperation::~LifecycleOperation() {
    m_owner.endLifecycleOperation();
}

auto FrameSourceCoordinator::registerSource(FrameSourceMode mode,
                                            std::shared_ptr<IFrameSource> source)
    -> std::expected<void, std::string> {
    if (!source) {
        return std::unexpected("frame source must not be null");
    }

    LifecycleOperation operation(*this);
    FrameCallback callback;
    {
        std::lock_guard lock(m_mutex);
        if (m_sources.contains(mode)) {
            return std::unexpected("frame source mode is already registered");
        }
        callback = m_callback;
    }
    if (callback) {
        source->setFrameCallback(callback);
    }
    {
        std::lock_guard lock(m_mutex);
        m_sources.emplace(mode, std::move(source));
        if (!m_activeMode.has_value()) {
            m_activeMode = mode;
        }
    }
    return {};
}

auto FrameSourceCoordinator::selectSource(FrameSourceMode mode)
    -> std::expected<void, std::string> {
    LifecycleOperation operation(*this);
    std::shared_ptr<IFrameSource> target;
    std::shared_ptr<IFrameSource> current;
    std::vector<std::shared_ptr<IFrameSource>> inactiveSources;
    {
        std::lock_guard lock(m_mutex);
        const auto targetEntry = m_sources.find(mode);
        if (targetEntry == m_sources.end()) {
            return std::unexpected("requested frame source is not registered");
        }
        if (m_activeMode == mode) {
            return {};
        }
        target = targetEntry->second;
        current = activeSourceLocked();
        inactiveSources.reserve(m_sources.size());
        for (const auto& [registeredMode, source] : m_sources) {
            if (registeredMode != mode) {
                inactiveSources.push_back(source);
            }
        }
    }

    const auto initialized = target->init();
    if (!initialized.has_value()) {
        return initialized;
    }

    const auto resume = current && current->isRunning();
    for (const auto& source : inactiveSources) {
        if (source->isRunning()) {
            source->stop();
        }
    }

    {
        std::lock_guard lock(m_mutex);
        m_activeMode = mode;
    }
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
    LifecycleOperation operation(*this);
    std::shared_ptr<IFrameSource> source;
    {
        std::lock_guard lock(m_mutex);
        source = activeSourceLocked();
    }
    if (!source) {
        return std::unexpected("no active frame source");
    }
    return source->init();
}

void FrameSourceCoordinator::start() {
    LifecycleOperation operation(*this);
    std::shared_ptr<IFrameSource> active;
    std::vector<std::shared_ptr<IFrameSource>> inactiveSources;
    {
        std::lock_guard lock(m_mutex);
        active = activeSourceLocked();
        for (const auto& [mode, source] : m_sources) {
            if (m_activeMode != mode) {
                inactiveSources.push_back(source);
            }
        }
    }
    if (!active) {
        return;
    }
    for (const auto& source : inactiveSources) {
        if (source->isRunning()) {
            source->stop();
        }
    }
    active->start();
}

void FrameSourceCoordinator::stop() {
    LifecycleOperation operation(*this);
    std::vector<std::shared_ptr<IFrameSource>> sources;
    {
        std::lock_guard lock(m_mutex);
        sources.reserve(m_sources.size());
        for (const auto& [mode, source] : m_sources) {
            static_cast<void>(mode);
            sources.push_back(source);
        }
    }
    for (const auto& source : sources) {
        if (source->isRunning()) {
            source->stop();
        }
    }
}

void FrameSourceCoordinator::setFrameCallback(FrameCallback callback) {
    LifecycleOperation operation(*this);
    std::vector<std::shared_ptr<IFrameSource>> sources;
    {
        std::lock_guard lock(m_mutex);
        m_callback = std::move(callback);
        sources.reserve(m_sources.size());
        for (const auto& [mode, source] : m_sources) {
            static_cast<void>(mode);
            sources.push_back(source);
        }
        callback = m_callback;
    }
    for (const auto& source : sources) {
        source->setFrameCallback(callback);
    }
}

bool FrameSourceCoordinator::isRunning() const {
    std::shared_ptr<IFrameSource> source;
    {
        std::lock_guard lock(m_mutex);
        source = activeSourceLocked();
    }
    return source && source->isRunning();
}

auto FrameSourceCoordinator::frameWidth() const -> uint32_t {
    std::shared_ptr<IFrameSource> source;
    {
        std::lock_guard lock(m_mutex);
        source = activeSourceLocked();
    }
    return source ? source->frameWidth() : 0U;
}

auto FrameSourceCoordinator::frameHeight() const -> uint32_t {
    std::shared_ptr<IFrameSource> source;
    {
        std::lock_guard lock(m_mutex);
        source = activeSourceLocked();
    }
    return source ? source->frameHeight() : 0U;
}

auto FrameSourceCoordinator::activeSourceLocked() const -> std::shared_ptr<IFrameSource> {
    if (!m_activeMode.has_value()) {
        return {};
    }
    const auto entry = m_sources.find(*m_activeMode);
    return entry == m_sources.end() ? std::shared_ptr<IFrameSource>{} : entry->second;
}

void FrameSourceCoordinator::beginLifecycleOperation() {
    std::unique_lock lock(m_mutex);
    m_operationCv.wait(lock, [this] { return !m_operationInProgress; });
    m_operationInProgress = true;
}

void FrameSourceCoordinator::endLifecycleOperation() noexcept {
    {
        std::lock_guard lock(m_mutex);
        m_operationInProgress = false;
    }
    m_operationCv.notify_one();
}

}  // namespace Dss::Acquisition
