#include "dss/processing/processing_pipeline.h"

namespace Dss::Processing {

void ProcessingPipeline::setBackend(std::unique_ptr<IProcessingStrategy> backend) {
    m_backend = std::move(backend);
}

auto ProcessingPipeline::process(const FramePacket& packet) -> ProcessingResult {
    if (!m_backend) {
        return {};
    }
    return m_backend->process(packet);
}

auto ProcessingPipeline::currentMode() const -> Dss::Core::ProcessingMode {
    return m_backend ? m_backend->mode() : Dss::Core::ProcessingMode::None;
}

auto ProcessingPipeline::backendName() const -> std::string_view {
    return m_backend ? m_backend->name() : std::string_view{"none"};
}

bool ProcessingPipeline::hasBackend() const {
    return m_backend != nullptr;
}

}  // namespace Dss::Processing
