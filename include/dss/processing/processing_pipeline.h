#pragma once

#include <memory>
#include <string_view>

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing {

class ProcessingPipeline {
public:
    void setBackend(std::unique_ptr<IProcessingStrategy> backend);

    [[nodiscard]] auto process(const FramePacket& packet) -> ProcessingResult;
    [[nodiscard]] auto currentMode() const -> Dss::Core::ProcessingMode;
    [[nodiscard]] auto backendName() const -> std::string_view;
    [[nodiscard]] bool hasBackend() const;

private:
    std::unique_ptr<IProcessingStrategy> m_backend;
};

}  // namespace Dss::Processing
