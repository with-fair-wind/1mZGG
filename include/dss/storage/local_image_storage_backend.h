#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dss/storage/i_storage_backend.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::Storage {

class LocalImageStorageBackend final : public IStorageBackend {
public:
    explicit LocalImageStorageBackend(std::filesystem::path baseDir)
        : m_baseDir(std::move(baseDir)) {}

    ~LocalImageStorageBackend() override {
        stop();
    }

    auto init(const std::filesystem::path& baseDir) -> std::expected<void, std::string> override {
        m_baseDir = baseDir;
        std::error_code error;
        std::filesystem::create_directories(m_baseDir, error);
        if (error) {
            m_ready.store(false);
            return std::unexpected("failed to create storage directory: " + error.message());
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

    auto start() -> std::expected<void, std::string> {
        if (!m_ready.load()) {
            return std::unexpected("storage backend is not initialized");
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
        return m_running;
    }

    auto enqueueRawFrame(std::filesystem::path relativePath, RawImageMetadata metadata,
                         std::span<const std::uint16_t> pixels)
        -> std::expected<void, std::string> {
        if (!m_ready.load()) {
            return std::unexpected("storage backend is not initialized");
        }
        if (!m_running) {
            return std::unexpected("storage worker is not running");
        }

        SaveRawFrameRequest request{};
        request.path = resolvePath(std::move(relativePath));
        request.metadata = std::move(metadata);
        request.pixels.assign(pixels.begin(), pixels.end());
        {
            std::lock_guard lock(m_queueMutex);
            m_queue.push_back(std::move(request));
        }
        m_queueCv.notify_one();
        return {};
    }

private:
    struct SaveRawFrameRequest {
        std::filesystem::path path;
        RawImageMetadata metadata{};
        std::vector<std::uint16_t> pixels;
    };

    [[nodiscard]] auto resolvePath(std::filesystem::path path) const -> std::filesystem::path {
        if (path.is_absolute()) {
            return path;
        }
        return m_baseDir / path;
    }

    void workerLoop(std::stop_token token) {
        while (true) {
            SaveRawFrameRequest request;
            {
                std::unique_lock lock(m_queueMutex);
                m_queueCv.wait(lock, token, [this, &token] {
                    return !m_queue.empty() || token.stop_requested();
                });
                if (m_queue.empty()) {
                    break;
                }
                request = std::move(m_queue.front());
                m_queue.pop_front();
            }
            writeRawFrame(request);
        }
    }

    static void writeRawFrame(const SaveRawFrameRequest& request) {
        std::error_code error;
        std::filesystem::create_directories(request.path.parent_path(), error);
        if (error) {
            return;
        }

        const auto bytes = buildRawImageFile(request.metadata, request.pixels);
        std::ofstream output(request.path, std::ios::binary);
        if (!output) {
            return;
        }
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    std::filesystem::path m_baseDir;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_running{false};
    std::jthread m_worker;
    std::mutex m_queueMutex;
    std::condition_variable_any m_queueCv;
    std::deque<SaveRawFrameRequest> m_queue;
};

}  // namespace Dss::Storage
