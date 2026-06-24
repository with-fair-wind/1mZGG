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

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/result_packet_utils.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/image_storage_format.h"
#include "dss/storage/track_data_storage_format.h"

namespace Dss::Storage {

/// 跟踪数据异步存储后端，将跟踪结果追加写入遗留格式文本文件
class TrackDataStorageBackend final : public IStorageBackend {
public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 跨线程消息总线类型
    /**
     * @brief 构造跟踪数据存储后端。
     * @param baseDir 默认存储根目录。
     * @param maxPendingRequests 写入队列允许的最大待处理批次数。
     */
    explicit TrackDataStorageBackend(std::filesystem::path baseDir,
                                     std::size_t maxPendingRequests = 1024)
        : m_baseDir(std::move(baseDir)), m_maxPendingRequests(maxPendingRequests) {}

    /// @brief 停止后台写入线程后销毁后端。
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

    /** @brief 查询后端是否已初始化。 @return 存储目录可用时返回 true。 */
    [[nodiscard]] bool isReady() const override {
        return m_ready.load();
    }

    /** @brief 获取当前存储根目录。 @return 根目录路径的常量引用。 */
    [[nodiscard]] auto baseDir() const -> const std::filesystem::path& {
        return m_baseDir;
    }

    /**
     * @brief 获取跟踪数据输出路径。
     * @return 会话 GAE 文件路径；未配置会话时返回默认文本路径。
     */
    [[nodiscard]] auto outputPath() const -> std::filesystem::path {
        return m_sessionOutputPath.empty() ? m_baseDir / "track_data.txt" : m_sessionOutputPath;
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

    /** @brief 查询后台写入状态。 @return 工作线程运行时返回 true。 */
    [[nodiscard]] bool isRunning() const {
        return m_running.load();
    }

    /**
     * @brief 获取累计丢弃请求数。
     * @return 因队列已满而拒绝的跟踪结果批次数。
     */
    [[nodiscard]] auto droppedRequests() const -> std::uint64_t {
        return m_droppedRequests.load();
    }
    /**
     * @brief 设置用于发布存储错误事件的消息总线。
     * @param bus 非拥有消息总线指针；应在启动工作线程前设置。
     */
    void setBus(MessageBus* bus) {
        m_bus = bus;
    }

    /** @brief 获取成功写入次数。 @return 成功追加到文件的记录批次数。 */
    [[nodiscard]] auto successfulWrites() const -> std::uint64_t {
        return m_successfulWrites.load();
    }

    /** @brief 获取失败写入次数。 @return 文件追加失败的记录批次数。 */
    [[nodiscard]] auto failedWrites() const -> std::uint64_t {
        return m_failedWrites.load();
    }

    /**
     * @brief 根据观测会话配置 GAE 输出文件。
     * @param naming 会话命名信息。
     * @return 配置成功时为空；工作线程运行或目录创建失败时返回错误描述。
     */
    auto configureSession(const ImageStorageNaming& naming) -> std::expected<void, std::string> {
        if (m_running.load()) {
            return std::unexpected(
                "cannot configure session while track storage worker is running");
        }
        const auto target = naming.searchMode ? std::string{"NNNNNN"} : naming.targetId;
        m_sessionOutputPath =
            m_baseDir / "GAE" /
            (naming.startTime + "_" + target + "_" + naming.observatoryId + ".GAE");
        std::error_code error;
        std::filesystem::create_directories(m_sessionOutputPath.parent_path(), error);
        if (error) {
            m_sessionOutputPath.clear();
            return std::unexpected("failed to create GAE session directory: " + error.message());
        }
        return {};
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
    /**
     * @brief 从跟踪结果事件构建遗留格式记录。
     * @param event 包含帧序号和目标列表的跟踪事件。
     * @return 由有效目标转换得到的数据记录列表。
     */
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

    /**
     * @brief 从队列取出记录批次并追加到磁盘。
     * @param token 用于停止等待并在队列排空后退出的令牌。
     */
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
            const auto result = writeRecords(records);
            if (result) {
                m_successfulWrites.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_failedWrites.fetch_add(1, std::memory_order_relaxed);
                if (m_bus != nullptr) {
                    m_bus->emit(Dss::Core::StorageWriteErrorEvent{
                        .backend = "track_data_storage",
                        .path = outputPath().string(),
                        .message = result.error(),
                    });
                }
            }
        }
    }

    /**
     * @brief 将记录批次追加到当前输出文件。
     * @param records 待格式化并写入的数据记录。
     * @return 写入成功时为空；目录或文件操作失败时返回错误描述。
     */
    auto writeRecords(const std::vector<TrackDataRecord>& records) const
        -> std::expected<void, std::string> {
        std::error_code error;
        std::filesystem::create_directories(outputPath().parent_path(), error);
        if (error) {
            return std::unexpected("failed to create track data directory: " + error.message());
        }

        std::ofstream output(outputPath(), std::ios::app);
        if (!output) {
            return std::unexpected("failed to open track data file");
        }
        for (const auto& record : records) {
            output << formatLegacyTrackDataLine(record) << '\n';
        }
        if (!output) {
            return std::unexpected("failed to write track data file");
        }
        return {};
    }

    std::filesystem::path m_baseDir;                   ///< 存储根目录
    std::atomic<bool> m_ready{false};                  ///< 是否已完成初始化
    std::atomic<bool> m_running{false};                ///< 后台线程运行状态
    std::size_t m_maxPendingRequests = 1024;           ///< 写入队列容量上限
    std::atomic<std::uint64_t> m_droppedRequests{0};   ///< 被背压拒绝的请求数量
    std::atomic<std::uint64_t> m_successfulWrites{0};  ///< 成功写入批次数量
    std::atomic<std::uint64_t> m_failedWrites{0};      ///< 写入失败批次数量
    std::filesystem::path m_sessionOutputPath;         ///< 当前会话 GAE 输出路径
    MessageBus* m_bus = nullptr;                       ///< 非拥有事件总线指针
    std::jthread m_worker;                             ///< 后台写入工作线程
    std::mutex m_queueMutex;                           ///< 保护写入队列的互斥锁
    std::condition_variable_any m_queueCv;             ///< 写入队列条件变量
    std::deque<std::vector<TrackDataRecord>> m_queue;  ///< 待写入记录批次队列
};

}  // namespace Dss::Storage
