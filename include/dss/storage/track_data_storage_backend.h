#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <expected>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dss/core/events.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/track_data_storage_format.h"

namespace Dss::Storage {

class TrackDataStorageBackend final : public IStorageBackend {
public:
    explicit TrackDataStorageBackend(std::filesystem::path baseDir)
        : m_baseDir(std::move(baseDir)) {}

    ~TrackDataStorageBackend() override {
        stop();
    }

    auto init(const std::filesystem::path& baseDir) -> std::expected<void, std::string> override {
        m_baseDir = baseDir;
        std::error_code error;
        std::filesystem::create_directories(m_baseDir, error);
        if (error) {
            m_ready.store(false);
            return std::unexpected("failed to create track data directory: " + error.message());
        }
        m_ready.store(true);
        return {};
    }

    [[nodiscard]] bool isReady() const override {
        return m_ready.load();
    }

    [[nodiscard]] auto baseDir() const -> const std::filesystem::path& {
        return m_baseDir;
    }

    [[nodiscard]] auto outputPath() const -> std::filesystem::path {
        return m_baseDir / "track_data.txt";
    }

    auto start() -> std::expected<void, std::string> {
        if (!m_ready.load()) {
            return std::unexpected("track data storage backend is not initialized");
        }
        if (m_worker.joinable()) {
            return {};
        }

        m_running = true;
        m_worker = std::jthread([this](std::stop_token token) { workerLoop(token); });
        return {};
    }

    void stop() {
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_queueCv.notify_all();
            m_worker.join();
        }
        m_running = false;
    }

    [[nodiscard]] bool isRunning() const {
        return m_running.load();
    }

    auto enqueueTrackResult(const Dss::Core::TrackResultEvent& event)
        -> std::expected<void, std::string> {
        if (!m_ready.load()) {
            return std::unexpected("track data storage backend is not initialized");
        }
        if (!m_running.load()) {
            return std::unexpected("track data storage worker is not running");
        }

        auto records = buildRecords(event);
        if (records.empty()) {
            return {};
        }

        {
            std::lock_guard lock(m_queueMutex);
            m_queue.push_back(std::move(records));
        }
        m_queueCv.notify_one();
        return {};
    }

private:
    [[nodiscard]] static auto buildRecords(const Dss::Core::TrackResultEvent& event)
        -> std::vector<TrackDataRecord> {
        std::vector<TrackDataRecord> records;
        records.reserve(event.targets.size());

        const auto optic = Dss::Core::OpticParams{};
        const auto opticCenter = Dss::Core::Vec2f{optic.fovCenterX, optic.fovCenterY};
        for (const auto& target : event.targets) {
            if (target.frameInfos.empty()) {
                continue;
            }

            const auto& frame = target.frameInfos.back();
            if (!frame.valid) {
                continue;
            }

            const auto& blob = frame.measuredBlob;
            TrackDataRecord record{};
            record.frameSeq = frame.frameSeq != 0U ? frame.frameSeq : event.frameSeq;
            record.timestamp = frame.timestamp;
            record.fovCenterAe = frame.fovCenterAe;
            if (record.fovCenterAe.x == 0.0F && record.fovCenterAe.y == 0.0F) {
                record.fovCenterAe =
                    Dss::Core::Vec2f{blob.fovCenterAziModify, blob.fovCenterEleModify};
            }
            record.blobPosition = blob.centroid;
            record.opticCenter = frame.opticCenter;
            if (record.opticCenter.x == 0.0F && record.opticCenter.y == 0.0F) {
                record.opticCenter = opticCenter;
            }
            record.area = blob.area;
            record.exposureTimeMilliseconds = static_cast<double>(frame.exposureTime) * 1000.0;
            record.magnitude = frame.magnitude != 0.0F ? frame.magnitude : record.magnitude;
            records.push_back(record);
        }
        return records;
    }

    void workerLoop(std::stop_token token) {
        while (true) {
            std::vector<TrackDataRecord> records;
            {
                std::unique_lock lock(m_queueMutex);
                m_queueCv.wait(lock, token, [this, &token] {
                    return !m_queue.empty() || token.stop_requested();
                });
                if (m_queue.empty()) {
                    break;
                }
                records = std::move(m_queue.front());
                m_queue.pop_front();
            }
            writeRecords(records);
        }
    }

    void writeRecords(const std::vector<TrackDataRecord>& records) const {
        std::error_code error;
        std::filesystem::create_directories(outputPath().parent_path(), error);
        if (error) {
            return;
        }

        std::ofstream output(outputPath(), std::ios::app);
        if (!output) {
            return;
        }
        for (const auto& record : records) {
            output << formatLegacyTrackDataLine(record) << '\n';
        }
    }

    std::filesystem::path m_baseDir;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_running{false};
    std::jthread m_worker;
    std::mutex m_queueMutex;
    std::condition_variable_any m_queueCv;
    std::deque<std::vector<TrackDataRecord>> m_queue;
};

}  // namespace Dss::Storage
