#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
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
#include "dss/core/result_packet_utils.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/track_data_storage_format.h"

namespace Dss::Storage {

/// 跟踪数据异步存储后端，将跟踪结果追加写入遗留格式文本文件
class TrackDataStorageBackend final : public IStorageBackend {
public:
    /**
     * @brief 构造跟踪数据存储后端
     * @param baseDir 默认存储根目录
     */
    explicit TrackDataStorageBackend(std::filesystem::path baseDir, std::size_t maxPendingRequests = 1024)
        : m_baseDir(std::move(baseDir)), m_maxPendingRequests(maxPendingRequests) {}

    ~TrackDataStorageBackend() override {
        stop();
    }

    /**
     * @brief 初始化存储目录
     * @param baseDir 存储根目录路径
     * @return 成功时返回空值；目录创建失败时返回错误描述
     */
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

    /// 查询存储后端是否已初始化
    [[nodiscard]] bool isReady() const override {
        return m_ready.load();
    }

    /// 获取当前存储根目录
    [[nodiscard]] auto baseDir() const -> const std::filesystem::path& {
        return m_baseDir;
    }

    /// 获取跟踪数据输出文件的完整路径
    [[nodiscard]] auto outputPath() const -> std::filesystem::path {
        return m_baseDir / "track_data.txt";
    }

    /**
     * @brief 启动后台写入工作线程
     * @return 成功时返回空值；未初始化时返回错误描述
     */
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

    /// 停止后台写入工作线程并等待其退出
    void stop() {
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_queueCv.notify_all();
            m_worker.join();
        }
        m_running = false;
    }

    /// 查询后台写入线程是否正在运行
    [[nodiscard]] bool isRunning() const {
        return m_running.load();
    }

    [[nodiscard]] auto droppedRequests() const -> std::uint64_t {
        return m_droppedRequests.load();
    }

    /**
     * @brief 将跟踪结果事件加入异步写入队列
     * @param event 跟踪结果事件
     * @return 成功时返回空值；未初始化或未运行时返回错误描述
     */
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
            if (m_queue.size() >= m_maxPendingRequests) {
                m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
                return std::unexpected("track data storage queue is full");
            }
            m_queue.push_back(std::move(records));
        }
        m_queueCv.notify_one();
        return {};
    }

private:
    /// 从跟踪结果事件构建遗留格式数据记录列表
    [[nodiscard]] static auto buildRecords(const Dss::Core::TrackResultEvent& event)
        -> std::vector<TrackDataRecord> {
        std::vector<TrackDataRecord> records;
        records.reserve(event.targets.size());

        const auto optic = Dss::Core::OpticParams{};
        const auto opticCenter = Dss::Core::Vec2f{optic.fovCenterX, optic.fovCenterY};
        const auto packets = Dss::Core::makeResultPackets(event.targets);
        for (const auto& packet : packets) {
            if (!packet.valid) {
                continue;
            }

            auto record = makeTrackDataRecord(packet, opticCenter);
            if (record.frameSeq == 0U) {
                record.frameSeq = event.frameSeq;
            }
            records.push_back(record);
        }
        return records;
    }

    /// 后台工作线程主循环，从队列取出记录并写入磁盘
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

    /// 将多条跟踪数据记录追加写入输出文件
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

    std::filesystem::path m_baseDir;                   ///< 存储根目录
    std::atomic<bool> m_ready{false};                  ///< 是否已完成初始化
    std::atomic<bool> m_running{false};
    std::size_t m_maxPendingRequests = 1024;
    std::atomic<std::uint64_t> m_droppedRequests{0};  ///< 被背压拒绝的请求数量
    std::jthread m_worker;                             ///< 后台写入工作线程
    std::mutex m_queueMutex;                           ///< 保护写入队列的互斥锁
    std::condition_variable_any m_queueCv;             ///< 写入队列条件变量
    std::deque<std::vector<TrackDataRecord>> m_queue;  ///< 待写入记录批次队列
};

}  // namespace Dss::Storage
