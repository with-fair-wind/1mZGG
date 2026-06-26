#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

#include "dss/acquisition/sapera_frame_source.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: dss_sapera_smoke <camera.ccf> [frame-count] [timeout-seconds]\n";
        return 64;
    }
    const auto requiredFrames = argc > 2 ? std::max(1, std::atoi(argv[2])) : 10;
    const auto timeoutSeconds = argc > 3 ? std::max(1, std::atoi(argv[3])) : 15;

    std::mutex mutex;
    std::condition_variable ready;
    int receivedFrames = 0;
    Dss::Acquisition::SaperaFrameSource source({.ccfPath = argv[1]});
    source.setFrameCallback([&](Dss::Processing::FramePacket packet) {
        if (packet.rawImage.size() !=
            static_cast<std::size_t>(packet.width) * packet.height) {
            return;
        }
        std::scoped_lock lock(mutex);
        ++receivedFrames;
        ready.notify_all();
    });

    if (auto initialized = source.init(); !initialized) {
        std::cerr << "Sapera init failed: " << initialized.error() << '\n';
        return 2;
    }
    if (auto started = source.startCapture(); !started) {
        std::cerr << "Sapera start failed: " << started.error() << '\n';
        return 3;
    }

    std::unique_lock lock(mutex);
    const auto completed = ready.wait_for(
        lock, std::chrono::seconds(timeoutSeconds),
        [&] { return receivedFrames >= requiredFrames; });
    lock.unlock();
    source.stop();

    if (!completed) {
        std::cerr << "Sapera timeout: received " << receivedFrames << "/"
                  << requiredFrames << " frames\n";
        return 4;
    }
    std::cout << "Sapera smoke passed: " << receivedFrames << " frames, "
              << source.frameWidth() << 'x' << source.frameHeight() << '\n';
    return 0;
}