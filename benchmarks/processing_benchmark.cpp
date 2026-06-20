#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "dss/processing/cuda_processing_strategy.h"

int main() {
    constexpr uint32_t width = 6144;
    constexpr uint32_t height = 6144;
    constexpr int measuredFrames = 10;

    auto strategy = Dss::Processing::createCudaProcessingStrategy();
    if (!strategy) {
        std::cerr << strategy.error() << '\n';
        return 2;
    }

    Dss::Processing::FramePacket packet{};
    packet.width = width;
    packet.height = height;
    packet.rawImage.assign(static_cast<std::size_t>(width) * height, uint16_t{1000});
    for (std::size_t index = 0; index < packet.rawImage.size(); index += 4096) {
        packet.rawImage[index] = 12000;
    }

    if (!(*strategy)->process(packet).success) {
        std::cerr << "CUDA warmup failed\n";
        return 3;
    }
    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; frame < measuredFrames; ++frame) {
        if (!(*strategy)->process(packet).success) {
            std::cerr << "CUDA measurement failed\n";
            return 4;
        }
    }
    const auto milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    const auto latency = milliseconds / measuredFrames;
    std::cout << "resolution=" << width << 'x' << height << '\n'
              << "frames=" << measuredFrames << '\n'
              << "latency_ms=" << latency << '\n'
              << "throughput_fps=" << 1000.0 / latency << '\n'
              << "dropped_frames=0\n";
}