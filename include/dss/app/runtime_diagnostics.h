#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

#include "dss/core/event_bus.h"

namespace Dss::App {

struct RuntimeDiagnosticsSnapshot {
    std::uint64_t processingDroppedFrames = 0;
    std::uint64_t imageSuccessfulWrites = 0;
    std::uint64_t imageFailedWrites = 0;
    std::uint64_t imageDroppedRequests = 0;
    std::uint64_t trackSuccessfulWrites = 0;
    std::uint64_t trackFailedWrites = 0;
    std::uint64_t trackDroppedRequests = 0;
    std::uint64_t networkErrors = 0;
    std::uint64_t serialErrors = 0;
    std::uint64_t storageErrors = 0;
};

struct RuntimeDiagnosticsSources {
    std::function<std::uint64_t()> processingDroppedFrames;
    std::function<std::uint64_t()> imageSuccessfulWrites;
    std::function<std::uint64_t()> imageFailedWrites;
    std::function<std::uint64_t()> imageDroppedRequests;
    std::function<std::uint64_t()> trackSuccessfulWrites;
    std::function<std::uint64_t()> trackFailedWrites;
    std::function<std::uint64_t()> trackDroppedRequests;
};

class RuntimeDiagnostics {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    RuntimeDiagnostics(MessageBus& bus, RuntimeDiagnosticsSources sources);

    [[nodiscard]] auto snapshot() const -> RuntimeDiagnosticsSnapshot;

private:
    RuntimeDiagnosticsSources m_sources;
    std::atomic<std::uint64_t> m_networkErrors{0};
    std::atomic<std::uint64_t> m_serialErrors{0};
    std::atomic<std::uint64_t> m_storageErrors{0};
    std::vector<Dss::Evt::ScopedConnection> m_connections;
};

}  // namespace Dss::App
