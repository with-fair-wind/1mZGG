#include "dss/processing/diff_processing_strategy.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "dss/processing/display_stretch.h"

namespace Dss::Processing {

DiffProcessingStrategy::DiffProcessingStrategy(DiffProcessingOptions options)
    : m_options(options),
      m_labeler(LabelConfig{.minArea = options.minArea, .maxArea = options.maxArea}) {}

auto DiffProcessingStrategy::process(const FramePacket& input) -> ProcessingResult {
    ProcessingResult result{};
    const auto pixelCount = static_cast<std::size_t>(input.width) * input.height;
    if (input.width == 0 || input.height == 0 || input.rawImage.size() != pixelCount) {
        return result;
    }

    result.stats = computeImageStats(input.rawImage);
    result.displayImage.assign(pixelCount, std::uint8_t{0});

    const auto dimensionsChanged =
        input.width != m_previousWidth || input.height != m_previousHeight;
    if (m_previousImage.empty() || dimensionsChanged) {
        m_previousImage = input.rawImage;
        m_previousWidth = input.width;
        m_previousHeight = input.height;
        result.success = true;
        return result;
    }

    for (std::size_t index = 0; index < pixelCount; ++index) {
        const auto current = static_cast<int>(input.rawImage[index]);
        const auto previous = static_cast<int>(m_previousImage[index]);
        if (std::abs(current - previous) > static_cast<int>(m_options.threshold)) {
            result.displayImage[index] = std::uint8_t{255};
        }
    }

    result.targetBlobs = m_labeler.labelAndExtract(result.displayImage, input.width, input.height);
    m_previousImage = input.rawImage;
    result.success = true;
    return result;
}

auto DiffProcessingStrategy::name() const -> std::string_view {
    return "diff";
}

auto DiffProcessingStrategy::mode() const -> Dss::Core::ProcessingMode {
    return Dss::Core::ProcessingMode::Diff;
}

}  // namespace Dss::Processing
