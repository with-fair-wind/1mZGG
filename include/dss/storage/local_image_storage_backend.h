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
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dss/storage/i_storage_backend.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::Storage {

/// 本地 RAW 图像异步存储后端，通过后台线程写入磁盘
class LocalImageStorageBackend final : public IStorageBackend {
public:
    /**
     * @brief 构造本地图像存储后端
     * @param baseDir 默认存储根目录
     */
    explicit LocalImageStorageBackend(std::filesystem::path baseDir, std::size_t maxPendingRequests = 1024)
        : m_baseDir(std::move(baseDir)), m_maxPendingRequests(maxPendingRequests) {}

    ~LocalImageStorageBackend() override {
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
            return std::unexpected("failed to create storage directory: " + error.message());
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

    /**
     * @brief 启动后台写入工作线程
     * @return 成功时返回空值；未初始化时返回错误描述
     */
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
        return m_running;
    }

    [[nodiscard]] auto droppedRequests() const -> std::uint64_t {
        return m_droppedRequests.load();
    }

    /**
     * @brief 将 RAW 帧加入异步写入队列
     * @param relativePath 相对或绝对文件路径
     * @param metadata 帧元数据
     * @param pixels 16 位像素数据
     * @return 成功时返回空值；未初始化或未运行时返回错误描述
     */
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
            if (m_queue.size() >= m_maxPendingRequests) {
                m_droppedRequests.fetch_add(1, std::memory_order_relaxed);
                return std::unexpected("storage queue is full");
            }
            m_queue.push_back(std::move(request));
        }
        m_queueCv.notify_one();
        return {};
    }

private:
    /// 待写入的 RAW 帧请求
    struct SaveRawFrameRequest {
        std::filesystem::path path;         ///< 目标文件路径
        RawImageMetadata metadata{};        ///< 帧元数据
        std::vector<std::uint16_t> pixels;  ///< 16 位像素数据
    };

    /// 将相对路径解析为基于存储根目录的绝对路径
    [[nodiscard]] auto resolvePath(std::filesystem::path path) const -> std::filesystem::path {
        if (path.is_absolute()) {
            return path;
        }
        return m_baseDir / path;
    }

    /// 后台工作线程主循环，从队列取出请求并写入磁盘
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

    /// 将单帧 RAW 数据编码并写入指定路径
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

    std::filesystem::path m_baseDir;          ///< 存储根目录
    std::atomic<bool> m_ready{false};         ///< 是否已完成初始化
    std::atomic<bool> m_running{false};
    std::size_t m_maxPendingRequests = 1024;
    std::atomic<std::uint64_t> m_droppedRequests{0};  ///< 被背压拒绝的请求数量
    std::jthread m_worker;                    ///< 后台写入工作线程
    std::mutex m_queueMutex;                  ///< 保护写入队列的互斥锁
    std::condition_variable_any m_queueCv;    ///< 写入队列条件变量
    std::deque<SaveRawFrameRequest> m_queue;  ///< 待写入帧队列
};

}  // namespace Dss::Storage
