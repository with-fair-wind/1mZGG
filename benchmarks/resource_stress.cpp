#include <QCoreApplication>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off: psapi.h requires the Windows type declarations first.
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif

#include "dss/core/event/events.h"
#include "dss/core/service/service_registry.h"
#include "dss/network/endpoint/image_sender.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/storage/detail/async_write_queue.h"
#include "dss/ui/view_model/display_view_model.h"

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using Snapshot = Dss::Core::ResourceSnapshot;
using Raw = Dss::Processing::RawImageBuffer;
using SharedRaw = Dss::Processing::SharedRawImage;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Bounded waits keep a failed worker from hanging the tool indefinitely.
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool released = false;
    bool timedOut = false;
    void arrive() {
        std::unique_lock lock(mutex);
        entered = true;
        cv.notify_all();
        timedOut = !cv.wait_for(lock, 30s, [&] { return released; });
    }
    void wait() {
        std::unique_lock lock(mutex);
        require(cv.wait_for(lock, 10s, [&] { return entered; }), "worker did not enter gate");
    }
    void release() {
        std::lock_guard lock(mutex);
        released = true;
        cv.notify_all();
    }
};

Json resources(const Snapshot& s) {
    return {{"queued_items", s.queuedItems},          {"queued_bytes", s.queuedBytes},
            {"active_items", s.activeItems},          {"active_bytes", s.activeBytes},
            {"peak_queued_bytes", s.peakQueuedBytes}, {"completed_items", s.completedItems},
            {"replaced_items", s.replacedItems},      {"last_work_us", s.lastWorkMicroseconds},
            {"max_work_us", s.maxWorkMicroseconds},   {"last_stop_us", s.lastStopMicroseconds}};
}

Json memory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters))) {
        return {{"working_set_bytes", counters.WorkingSetSize},
                {"peak_working_set_bytes", counters.PeakWorkingSetSize},
                {"private_bytes", counters.PrivateUsage}};
    }
#endif
    return nullptr;
}

struct Frames {
    std::uint32_t width;
    std::uint32_t height;
    std::vector<std::weak_ptr<const Raw>> references;
    auto make(std::uint64_t seq) -> Dss::Processing::FramePacket {
        auto raw = std::make_shared<const Raw>(static_cast<std::size_t>(width) * height,
                                               static_cast<std::uint16_t>(seq));
        references.push_back(raw);
        Dss::Processing::FramePacket result;
        result.frameSeq = seq;
        result.width = width;
        result.height = height;
        result.rawImage = std::move(raw);
        return result;
    }
    void verifyReleased() const {
        require(std::ranges::all_of(references, [](const auto& weak) { return weak.expired(); }),
                "RAW payload retained after stop/reset");
    }
};

class GatedStrategy final : public Dss::Processing::IProcessingStrategy {
public:
    explicit GatedStrategy(Gate& gate) : m_gate(gate) {}
    auto process(const Dss::Processing::FramePacket&)
        -> Dss::Processing::ProcessingResult override {
        if (m_first) {
            m_first = false;
            m_gate.arrive();
        }
        Dss::Processing::ProcessingResult result;
        result.success = true;
        result.rawStatsValid = true;
        return result;
    }
    auto name() const -> std::string_view override {
        return "resource-stress-gate";
    }
    auto mode() const -> Dss::Core::ProcessingMode override {
        return Dss::Core::ProcessingMode::Direct;
    }

private:
    Gate& m_gate;
    bool m_first = true;
};

Json processing(Frames& frames, std::size_t count) {
    Dss::Core::MessageBus bus;
    Gate gate;
    Dss::Processing::ImageProcessor processor(bus);
    processor.setProcessingStrategy(std::make_unique<GatedStrategy>(gate));
    processor.setCpuDisplayImageRequired(false);
    processor.start();
    require(processor.submitFrame(frames.make(0)), "first processing admission failed");
    gate.wait();
    std::size_t accepted = 1;
    for (std::size_t index = 1; index < count; ++index) {
        accepted += processor.submitFrame(frames.make(index)) ? 1 : 0;
    }
    const auto blocked = processor.resourceSnapshot();
    const auto blockedMemory = memory();
    gate.release();
    processor.drain();
    const auto stopped = processor.resourceSnapshot();
    require(!gate.timedOut && !processor.hasFailed(), "processing failed or gate timed out");
    require(blocked.queuedItems == 4 && blocked.activeItems == 1,
            "processing queue not bounded to four");
    require(
        stopped.queuedBytes == 0 && stopped.activeBytes == 0 && stopped.completedItems == accepted,
        "processing drain did not release or complete accepted frames");
    frames.verifyReleased();
    return {{"blocked", resources(blocked)},
            {"stopped", resources(stopped)},
            {"accepted", accepted},
            {"dropped", processor.droppedFrames()},
            {"blocked_memory", blockedMemory}};
}

Json storage(Frames& frames, std::size_t count, std::size_t budget) {
    Gate gate;
    Dss::Storage::AsyncWriteQueue<SharedRaw> queue(1024, budget);
    bool first = true;
    require(queue
                .start([&](const SharedRaw&) -> std::expected<void, std::string> {
                    if (first) {
                        first = false;
                        gate.arrive();
                    }
                    return {};
                })
                .has_value(),
            "storage start failed");
    std::size_t accepted = 0;
    for (std::size_t index = 0; index < count; ++index) {
        auto frame = frames.make(index);
        const auto bytes = frame.rawImage->capacity() * sizeof(std::uint16_t);
        accepted += queue.enqueue(std::move(frame.rawImage), bytes).has_value() ? 1 : 0;
        if (index == 0) {
            require(accepted == 1, "budget cannot hold first frame");
            gate.wait();
        }
    }
    const auto blocked = queue.resourceSnapshot();
    const auto blockedMemory = memory();
    queue.requestStop();
    gate.release();
    queue.stop();
    const auto stopped = queue.resourceSnapshot();
    require(!gate.timedOut, "storage gate timed out");
    require(blocked.queuedBytes + blocked.activeBytes <= budget, "storage byte budget exceeded");
    require(stopped.queuedBytes == 0 && stopped.activeBytes == 0 &&
                queue.successfulWrites() == accepted,
            "storage drain lost an accepted frame or retained payload");
    frames.verifyReleased();
    return {{"blocked", resources(blocked)},
            {"stopped", resources(stopped)},
            {"accepted", accepted},
            {"dropped", queue.droppedRequests()},
            {"blocked_memory", blockedMemory}};
}

Json display(Frames& frames, std::size_t count) {
    Dss::Core::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::DisplayViewModel model({.bus = bus, .registry = registry});
    std::jthread producer([&] {
        for (std::size_t index = 0; index < count; ++index) {
            auto frame = frames.make(index);
            Dss::Core::DisplayRefreshEvent event{};
            event.frameSeq = index;
            event.width = frames.width;
            event.height = frames.height;
            event.stride = frames.width;
            event.rawImage = std::move(frame.rawImage);
            bus.emit(event);
        }
    });
    producer.join();
    const auto blocked = model.resourceSnapshot();
    const auto blockedMemory = memory();
    QCoreApplication::processEvents();
    const auto delivered = model.resourceSnapshot();
    model.clearCurrentDisplayFrame();
    const auto stopped = model.resourceSnapshot();
    require(blocked.queuedItems == 1 && blocked.replacedItems == count - 1,
            "display coalescing failed");
    require(delivered.queuedItems == 0 && delivered.completedItems == 1, "display delivery failed");
    require(stopped.queuedBytes == 0 && stopped.activeBytes == 0, "display reset retained payload");
    frames.verifyReleased();
    return {{"blocked", resources(blocked)},
            {"delivered", resources(delivered)},
            {"stopped", resources(stopped)},
            {"blocked_memory", blockedMemory}};
}

Json network(Frames& frames, std::size_t count) {
    Dss::Core::MessageBus bus;
    Gate gate;
    Dss::Network::ImageSender sender(bus);
    require(
        sender
            .open(
                {.localIp = "127.0.0.1", .localPort = 0, .remoteIp = "127.0.0.1", .remotePort = 9})
            .has_value(),
        "sender open failed");
    for (std::size_t index = 0; index < count; ++index) {
        auto frame = frames.make(index);
        const auto bytes = frame.rawImage->capacity() * sizeof(std::uint16_t);
        // Exercise the real worker and latest-only slot; no datagrams are sent.
        sender.submitForSend(
            index, {},
            [raw = std::move(frame.rawImage), &gate, index] {
                if (index == 0) {
                    gate.arrive();
                }
                return std::shared_ptr<const std::vector<std::uint8_t>>{};
            },
            frames.width, frames.height, bytes);
        if (index == 0) {
            gate.wait();
        }
    }
    const auto blocked = sender.resourceSnapshot();
    const auto blockedMemory = memory();
    gate.release();
    sender.close();
    const auto stopped = sender.resourceSnapshot();
    require(!gate.timedOut && blocked.queuedItems == 1 && blocked.activeItems == 1 &&
                blocked.replacedItems == count - 2,
            "sender slot bounds failed");
    require(stopped.queuedBytes == 0 && stopped.activeBytes == 0, "sender close retained payload");
    frames.verifyReleased();
    return {{"blocked", resources(blocked)},
            {"stopped", resources(stopped)},
            {"blocked_memory", blockedMemory}};
}

std::size_t number(const char* text, std::size_t low, std::size_t high) {
    std::size_t used = 0;
    const auto value = std::stoull(text, &used);
    require(used == std::string(text).size() && value >= low && value <= high,
            "argument out of range");
    return static_cast<std::size_t>(value);
}
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        require(argc == 1 || argc == 6,
                "usage: dss_resource_stress [width height frames cycles budgetMiB]");
        const auto width = argc > 1 ? number(argv[1], 1, 6144) : 512;
        const auto height = argc > 1 ? number(argv[2], 1, 6144) : 512;
        const auto count = argc > 1 ? number(argv[3], 6, 10000) : 100;
        const auto cycles = argc > 1 ? number(argv[4], 1, 100) : 3;
        const auto budget = (argc > 1 ? number(argv[5], 1, 512) : 256) * 1024U * 1024U;
        require(width * height * 2 <= budget, "budget must hold at least one RAW frame");
        Json report{{"schema_version", 1},
                    {"kind", "controlled_backpressure"},
                    {"width", width},
                    {"height", height},
                    {"frames_per_scenario", count},
                    {"cycles", cycles},
                    {"storage_budget_bytes", budget},
                    {"memory_before", memory()},
#ifdef NDEBUG
                    {"build_type", "Release"},
#else
                    {"build_type", "Debug"},
#endif
                    {"runs", Json::array()}};
        for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
            Frames frames{
                static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
            Json run;
            run["processing"] = processing(frames, count);
            run["storage"] = storage(frames, count, budget);
            run["display"] = display(frames, count);
            run["network_factory"] = network(frames, count);
            run["memory_after_cycle"] = memory();
            report["runs"].push_back(std::move(run));
        }
        for (const auto* component : {"processing", "storage", "network_factory"}) {
            std::vector<std::uint64_t> samples;
            for (const auto& run : report["runs"]) {
                samples.push_back(run[component]["stopped"]["last_stop_us"].get<std::uint64_t>());
            }
            std::ranges::sort(samples);
            // Nearest-rank percentiles across lifecycle cycles, after gate release.
            const auto rank = [&](std::size_t percentile) {
                return samples[(samples.size() * percentile + 99U) / 100U - 1U];
            };
            report["stop_after_release_us"][component] = {{"samples", samples.size()},
                                                          {"p50", rank(50)},
                                                          {"p95", rank(95)},
                                                          {"max", samples.back()}};
        }
        report["invariants_passed"] = true;
        report["memory_after"] = memory();
        std::cout << report.dump(2) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << Json{{"invariants_passed", false}, {"error", error.what()}}.dump() << '\n';
        return EXIT_FAILURE;
    }
}
