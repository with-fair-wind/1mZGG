/**
 * @file
 * @brief GEO 多目标、实际文件写入和 UDP 回环基线。
 * @details 参数：新建输出目录、帧数、目标数、RAW 边长、RAW 帧数、可选帧周期（微秒）。
 * 每个 UDP 报文等待一次接收并逐字节核验，测量受控负载延迟，不代表链路饱和吞吐。
 * 文件由生产存储后端写入并保留；未执行 fsync，不代表断电持久化或裸盘带宽。
 */
#include <QCoreApplication>
#include <QUdpSocket>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off: Windows types must precede psapi.h.
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif

#include "dss/core/result/result_packet_utils.h"
#include "dss/network/endpoint/data_exchange.h"
#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#include "dss/tracking/strategy/geo_tracker.h"

namespace {
using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

Json percentiles(std::vector<double> samples) {
    std::ranges::sort(samples);
    if (samples.empty())
        return nullptr;
    return {{"p50_us", samples[(samples.size() - 1) * 50 / 100]},
            {"p95_us", samples[(samples.size() - 1) * 95 / 100]},
            {"p99_us", samples[(samples.size() - 1) * 99 / 100]},
            {"max_us", samples.back()}};
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

Json queue(const Dss::Core::ResourceSnapshot& snapshot) {
    return {{"peak_queued_bytes", snapshot.peakQueuedBytes},
            {"completed_items", snapshot.completedItems},
            {"max_work_us", snapshot.maxWorkMicroseconds},
            {"stop_us", snapshot.lastStopMicroseconds},
            {"remaining_items", snapshot.queuedItems + snapshot.activeItems}};
}

int number(char* value, int maximum) {
    std::size_t consumed = 0;
    const auto result = std::stoi(value, &consumed);
    require(consumed == std::string(value).size() && result >= 1 && result <= maximum,
            "argument outside supported range");
    return result;
}
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        require(argc == 6 || argc == 7,
                "usage: dss_tracking_io_baseline NEW_OUTPUT_DIR FRAMES TARGETS RAW_SIDE RAW_FRAMES "
                "[FRAME_PERIOD_US]");
        const auto output = std::filesystem::absolute(argv[1]);
        const int frames = number(argv[2], 1000000);
        const int targetCount = number(argv[3], 16);
        const int side = number(argv[4], 6144);
        const int rawFrames = number(argv[5], 256);
        const int framePeriodUs = argc == 7 ? number(argv[6], 1000000) : 0;
        require(frames >= 4 && rawFrames <= frames, "require FRAMES >= 4 and RAW_FRAMES <= FRAMES");
        require(std::filesystem::create_directory(output),
                "output directory must be new and its parent must exist");

        Json result{{"frames", frames},
                    {"targets", targetCount},
                    {"frame_period_us", framePeriodUs},
                    {"raw_side", side},
                    {"raw_frames", rawFrames},
                    {"qt_version", qVersion()},
                    {"compiler", DSS_BASELINE_COMPILER},
#ifdef NDEBUG
                    {"build", "Release"},
#else
                    {"build", "Debug"},
#endif
                    {"memory_start", memory()}};
        Dss::Core::MessageBus bus;
        QUdpSocket receiver;
        require(receiver.bind(QHostAddress(QHostAddress::LocalHost), 0),
                "UDP receiver bind failed");
        Dss::Network::UdpEndpointConfig endpoint;
        endpoint.localIp = "127.0.0.1";
        endpoint.localPort = 0;
        endpoint.remoteIp = "127.0.0.1";
        endpoint.remotePort = receiver.localPort();
        Dss::Network::DataExchange exchange(bus);
        require(exchange.open(endpoint, endpoint).has_value(), "data exchange open failed");
        Dss::Storage::TrackDataStorageBackend tracks(output / "tracks");
        Dss::Storage::LocalImageStorageBackend images(output / "raw");
        require(tracks.init(output / "tracks").has_value() && tracks.start().has_value(),
                "track storage start failed");
        require(images.init(output / "raw").has_value() && images.start().has_value(),
                "image storage start failed");
        Dss::Core::TrackingSettings settings;
        settings.searchRadius = 20;
        settings.opticParams.imageWidth = frames * 5 + targetCount * 100 + 1000;
        settings.opticParams.imageHeight = frames * 4 + targetCount * 100 + 1000;
        Dss::Tracking::GeoTracker tracker(settings);
        std::vector<double> trackTimes, udpTimes, frameTimes;
        trackTimes.reserve(frames);
        udpTimes.reserve(frames);
        frameTimes.reserve(frames);
        std::size_t maxTargets = 0, maxHistory = 0, trackAccepted = 0, rawAccepted = 0,
                    received = 0;
        std::vector<std::string> targetIds;
        const auto start = Clock::now();
        for (int index = 0; index < frames; ++index) {
            const auto frameStart = Clock::now();
            Dss::Core::FrameMeasurements frame;
            frame.frameSeq = static_cast<std::uint64_t>(index + 1);
            frame.frameFreq = 1;
            frame.timestamp = {.year = 2026, .month = 9, .day = 22};
            for (int item = 0; item < targetCount; ++item) {
                Dss::Core::MeasuredBlob blob;
                blob.centroid = {100.0F + item * 100 + index * 5, 100.0F + item * 100 + index * 4};
                blob.posAe = {blob.centroid.x * 0.01F, blob.centroid.y * 0.01F};
                blob.area = 16;
                frame.targetBlobs.push_back(blob);
            }
            const auto trackStart = Clock::now();
            const auto targets = tracker.track(frame);
            trackTimes.push_back(elapsed(trackStart));
            if (index >= 3)
                require(targets.size() == static_cast<std::size_t>(targetCount),
                        "unexpected target count");
            maxTargets = (std::max)(maxTargets, targets.size());
            for (std::size_t item = 0; item < targets.size(); ++item) {
                const auto& target = targets[item];
                if (index == 3)
                    targetIds.push_back(target.targetId);
                require(target.living && target.targetId == targetIds[item],
                        "target retired or changed identity");
                require(target.frameInfos.size() <=
                            static_cast<std::size_t>((std::max)(10, settings.numFramesLiving)),
                        "history window exceeded");
                require(target.totalFrameCount() == frame.frameSeq, "cumulative count changed");
                maxHistory = (std::max)(maxHistory, target.frameInfos.size());
            }
            const auto packets = Dss::Core::makeResultPackets(targets);
            if (!packets.empty()) {
                const auto metadata = Dss::Network::makeGxtcMetadata(packets.front());
                const auto mapped = Dss::Network::makeGxtcTargets(packets);
                const auto expected = Dss::Network::buildGxtcPacket(metadata, mapped);
                const auto udpStart = Clock::now();
                const auto sent = exchange.sendGxtc(metadata, mapped);
                require(sent && *sent == static_cast<std::int64_t>(expected.size()),
                        "UDP send failed");
                require(receiver.hasPendingDatagrams() || receiver.waitForReadyRead(1000),
                        "UDP receive timed out");
                QByteArray data;
                data.resize(static_cast<qsizetype>(receiver.pendingDatagramSize()));
                require(receiver.readDatagram(data.data(), data.size()) == data.size(),
                        "UDP read failed");
                require(static_cast<std::size_t>(data.size()) == expected.size() &&
                            std::equal(expected.begin(), expected.end(),
                                       reinterpret_cast<const std::uint8_t*>(data.constData())),
                        "UDP content mismatch");
                udpTimes.push_back(elapsed(udpStart));
                ++received;
                if (tracks.enqueueTrackResult({frame.frameSeq, targets}))
                    ++trackAccepted;
            }
            if (index < rawFrames) {
                Dss::Storage::RawImageMetadata metadata;
                metadata.width = side;
                metadata.height = side;
                const std::vector<std::uint16_t> pixels(static_cast<std::size_t>(side) * side,
                                                        static_cast<std::uint16_t>(index));
                if (images.enqueueRawFrame(std::to_string(index) + ".raw", metadata, pixels))
                    ++rawAccepted;
            }
            frameTimes.push_back(elapsed(frameStart));
            if (framePeriodUs > 0) {
                std::this_thread::sleep_until(
                    start + std::chrono::microseconds{static_cast<std::int64_t>(index + 1) *
                                                      framePeriodUs});
            }
        }
        const auto stopStart = Clock::now();
        tracks.stop();
        images.stop();
        exchange.close();
        const auto stopUs = elapsed(stopStart);
        const auto totalUs = elapsed(start);
        require(tracks.failedWrites() == 0 && images.failedWrites() == 0, "file write failed");
        require(trackAccepted + tracks.droppedRequests() == static_cast<std::size_t>(frames - 3) &&
                    rawAccepted + images.droppedRequests() == static_cast<std::size_t>(rawFrames),
                "unexpected enqueue failure");
        require(
            tracks.successfulWrites() == trackAccepted && images.successfulWrites() == rawAccepted,
            "accepted write lost");
        std::size_t trackLines = 0;
        std::ifstream records(tracks.outputPath());
        for (std::string line; std::getline(records, line);)
            ++trackLines;
        require(trackLines == trackAccepted * targetCount, "track archive record count mismatch");
        std::uintmax_t fileBytes = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(output)) {
            if (entry.is_regular_file())
                fileBytes += entry.file_size();
        }
        result.update({{"tracking", percentiles(trackTimes)},
                       {"udp_round_trip", percentiles(udpTimes)},
                       {"frame_submission", percentiles(frameTimes)},
                       {"elapsed_us", totalUs},
                       {"observed_frames_per_second", frames * 1000000.0 / totalUs},
                       {"stop_us", stopUs},
                       {"max_targets", maxTargets},
                       {"max_history_per_target", maxHistory},
                       {"udp_received", received},
                       {"track_accepted", trackAccepted},
                       {"track_rejected", tracks.droppedRequests()},
                       {"track_archive_lines", trackLines},
                       {"raw_accepted", rawAccepted},
                       {"raw_rejected", images.droppedRequests()},
                       {"files_bytes", fileBytes},
                       {"track_queue", queue(tracks.resourceSnapshot())},
                       {"raw_queue", queue(images.resourceSnapshot())},
                       {"memory_end", memory()}});
        std::ofstream report(output / "report.json");
        report << result.dump(2) << '\n';
        require(static_cast<bool>(report), "report write failed");
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
