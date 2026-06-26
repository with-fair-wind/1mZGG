#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "dss/processing/cuda_processing_strategy.h"
#ifdef DSS_HAS_OPENCV
#include "dss/processing/opencv_processing_strategy.h"
#endif

namespace {

auto measure(std::string_view name, Dss::Processing::IProcessingStrategy& strategy,
             const Dss::Processing::FramePacket& packet, int frames) -> bool {
    if (!strategy.process(packet).success) {
        std::cerr << name << " warmup failed\n";
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frames; ++frame) {
        if (!strategy.process(packet).success) {
            std::cerr << name << " measurement failed\n";
            return false;
        }
    }
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    const auto latency = elapsed / frames;
    std::cout << name << ".latency_ms=" << latency << '\n'
              << name << ".throughput_fps=" << 1000.0 / latency << '\n'
              << name << ".dropped_frames=0\n";
    return true;
}

}  // namespace

int main() {
    constexpr uint32_t width = 6144;
    constexpr uint32_t height = 6144;
    constexpr int measuredFrames = 10;
    constexpr std::size_t bytesPerCudaPixel =
        sizeof(uint16_t) + 2 * sizeof(uint8_t);

    auto cuda = Dss::Processing::createCudaProcessingStrategy();
    if (!cuda) {
        std::cerr << cuda.error() << '\n';
        return 2;
    }

    Dss::Processing::FramePacket packet{};
    packet.width = width;
    packet.height = height;
    packet.rawImage.assign(static_cast<std::size_t>(width) * height, uint16_t{1000});
    for (std::size_t index = 0; index < packet.rawImage.size(); index += 4096) {
        packet.rawImage[index] = 12000;
    }

    std::cout << "resolution=" << width << 'x' << height << '\n'
              << "frames=" << measuredFrames << '\n'
              << "cuda.device_buffer_bytes="
              << packet.rawImage.size() * bytesPerCudaPixel << '\n';
#ifdef DSS_HAS_OPENCV
    Dss::Processing::OpenCvProcessingStrategy cpu;
    if (!measure("opencv", cpu, packet, measuredFrames)) {
        return 3;
    }
#endif
    if (!measure("cuda", **cuda, packet, measuredFrames)) {
        return 4;
    }
    return 0;
}
