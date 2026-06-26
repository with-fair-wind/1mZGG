#include "dss/processing/cuda_processing_strategy.h"

#ifdef DSS_HAS_CUDA

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

#include "dss/gpu/cuda_device_manager.h"
#include "dss/gpu/cuda_kernels.h"
#include "dss/gpu/gpu_buffer.h"
#include "dss/processing/display_stretch.h"
#include "dss/processing/labeler.h"

namespace Dss::Processing {
namespace {

class CudaProcessingStrategy final : public IProcessingStrategy {
public:
    CudaProcessingStrategy(CudaProcessingOptions options,
                           std::unique_ptr<Dss::Gpu::CudaDeviceManager> device)
        : m_options(options),
          m_device(std::move(device)),
          m_labeler(LabelConfig{.minArea = options.minArea, .maxArea = options.maxArea}) {}

    auto process(const FramePacket& input) -> ProcessingResult override {
        ProcessingResult result{};
        const auto pixelCount = static_cast<std::size_t>(input.width) * input.height;
        if (input.width == 0 || input.height == 0 || input.rawImage.size() != pixelCount ||
            pixelCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return result;
        }

        try {
            ensureCapacity(pixelCount);
            result.stats = computeImageStats(input.rawImage);
            result.displayImage.resize(pixelCount);
            std::vector<uint8_t> binary(pixelCount);

            const auto stream = m_device->stream();
            m_input.upload(input.rawImage, stream);
            const auto stretchRange =
                std::max(1, static_cast<int>(m_options.displayHigh) - m_options.displayLow);
            Dss::Gpu::short_to_byte(m_input.devicePtr(), m_display.devicePtr(),
                                    m_options.displayLow, m_options.displayHigh,
                                    static_cast<float>(stretchRange),
                                    static_cast<int>(pixelCount), stream);

            const auto thresholdValue = std::clamp(
                result.stats.avg + m_options.thresholdSigma * result.stats.stdDev,
                0.0,
                static_cast<double>(std::numeric_limits<uint16_t>::max()));
            Dss::Gpu::binary16(m_input.devicePtr(), m_binary.devicePtr(),
                               static_cast<uint16_t>(std::lround(thresholdValue)),
                               static_cast<int>(pixelCount), stream);

            if (cudaGetLastError() != cudaSuccess) {
                return result;
            }
            m_display.download(result.displayImage, stream);
            m_binary.download(binary, stream);
            m_device->synchronize();

            result.targetBlobs = m_labeler.labelAndExtract(binary, input.width, input.height);
            result.success = true;
        } catch (const std::runtime_error&) {
            return {};
        }
        return result;
    }

    [[nodiscard]] auto name() const -> std::string_view override {
        return "cuda";
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }

private:
    void ensureCapacity(std::size_t pixelCount) {
        if (m_input.size() == pixelCount) {
            return;
        }
        m_input = Dss::Gpu::GpuBuffer<uint16_t>(pixelCount);
        m_display = Dss::Gpu::GpuBuffer<uint8_t>(pixelCount);
        m_binary = Dss::Gpu::GpuBuffer<uint8_t>(pixelCount);
    }

    CudaProcessingOptions m_options;
    std::unique_ptr<Dss::Gpu::CudaDeviceManager> m_device;
    Labeler m_labeler;
    Dss::Gpu::GpuBuffer<uint16_t> m_input;
    Dss::Gpu::GpuBuffer<uint8_t> m_display;
    Dss::Gpu::GpuBuffer<uint8_t> m_binary;
};

}  // namespace

auto createCudaProcessingStrategy(CudaProcessingOptions options)
    -> std::expected<std::unique_ptr<IProcessingStrategy>, std::string> {
    auto device = std::make_unique<Dss::Gpu::CudaDeviceManager>();
    if (auto initialized = device->init(); !initialized) {
        return std::unexpected(initialized.error());
    }
    return std::unique_ptr<IProcessingStrategy>(
        std::make_unique<CudaProcessingStrategy>(options, std::move(device)));
}

}  // namespace Dss::Processing

#endif