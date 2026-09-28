#include "dss/app/runtime_diagnostics.h"

#include <utility>

#include "dss/core/event/events.h"

namespace Dss::App {
namespace {

[[nodiscard]] auto readCounter(const std::function<std::uint64_t()>& reader) -> std::uint64_t {
    return reader ? reader() : 0U;
}

auto readResources(const std::function<Dss::Core::ResourceSnapshot()>& reader)
    -> Dss::Core::ResourceSnapshot {
    return reader ? reader() : Dss::Core::ResourceSnapshot{};
}

}  // namespace

RuntimeDiagnostics::RuntimeDiagnostics(MessageBus& bus, RuntimeDiagnosticsSources sources)
    : m_sources(std::move(sources)) {
    m_connections.push_back(bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>(
        [this](const auto&) { m_networkErrors.fetch_add(1, std::memory_order_relaxed); }));
    m_connections.push_back(bus.subscribe<Dss::Core::SerialFrameErrorEvent>(
        [this](const auto&) { m_serialErrors.fetch_add(1, std::memory_order_relaxed); }));
    m_connections.push_back(bus.subscribe<Dss::Core::SerialDecodeErrorEvent>(
        [this](const auto&) { m_serialErrors.fetch_add(1, std::memory_order_relaxed); }));
    m_connections.push_back(bus.subscribe<Dss::Core::StorageWriteErrorEvent>(
        [this](const auto&) { m_storageErrors.fetch_add(1, std::memory_order_relaxed); }));
}

auto RuntimeDiagnostics::snapshot() const -> RuntimeDiagnosticsSnapshot {
    return {
        .processingDroppedFrames = readCounter(m_sources.processingDroppedFrames),
        .imageSuccessfulWrites = readCounter(m_sources.imageSuccessfulWrites),
        .imageFailedWrites = readCounter(m_sources.imageFailedWrites),
        .imageDroppedRequests = readCounter(m_sources.imageDroppedRequests),
        .trackSuccessfulWrites = readCounter(m_sources.trackSuccessfulWrites),
        .trackFailedWrites = readCounter(m_sources.trackFailedWrites),
        .trackDroppedRequests = readCounter(m_sources.trackDroppedRequests),
        .networkErrors = m_networkErrors.load(std::memory_order_relaxed),
        .serialErrors = m_serialErrors.load(std::memory_order_relaxed),
        .storageErrors = m_storageErrors.load(std::memory_order_relaxed),
        .processing = readResources(m_sources.processing),
        .imageStorage = readResources(m_sources.imageStorage),
        .trackStorage = readResources(m_sources.trackStorage),
        .imageSender = readResources(m_sources.imageSender),
    };
}

}  // namespace Dss::App
