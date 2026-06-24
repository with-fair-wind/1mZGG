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
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/storage/bmp_image_format.h"
#include "dss/storage/i_storage_backend.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::Storage {

/// 本地 RAW 图像异步存储后端，通过后台线程写入磁盘
class LocalImageStorageBackend final : public IStorageBackend {
public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 跨线程消息总线类型
    /**
     * @brief 构造本地图像存储后端。
     * @param baseDir 默认存储根目录。
     * @param maxPendingRequests 写入队列允许的最大待处理请求数。
     */
    explicit LocalImageStorageBackend(std::filesystem::path baseDir,
                                      std::size_t maxPendingRequests = 1024)
        : m_baseDir(std::move(baseDir)), m_maxPendingRequests(maxPendingRequests) {}

    /// @brief 停止后台写入线程后销毁后端。
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

    /** @brief 查询后端是否已初始化。 @return 存储根目录可用时返回 true。 */
    [[nodiscard]] bool isReady() const override {
        return m_ready.load();
    }

    /** @brief 获取当前存储根目录。 @return 根目录路径的常量引用。 */
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

    /** @brief 查询后台写入状态。 @return 工作线程运行时返回 true。 */
    [[nodiscard]] bool isRunning() const {
        return m_running;
    }

    /**
     * @brief 获取累计丢弃请求数。
     * @return 因写入队列已满而拒绝的请求数量。
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

    /** @brief 获取成功写入次数。 @return 后台成功完成的帧写入数量。 */
    [[nodiscard]] auto successfulWrites() const -> std::uint64_t {
        return m_successfulWrites.load();
    }

    /** @brief 获取失败写入次数。 @return 后台写入失败的帧数量。 */
    [[nodiscard]] auto failedWrites() const -> std::uint64_t {
        return m_failedWrites.load();
    }

    /** @brief 查询是否已配置观测会话。 @return 会话命名配置存在时返回 true。 */
    [[nodiscard]] bool hasSession() const {
        return m_sessionNaming.has_value();
    }

    /**
     * @brief 获取当前会话目录。
     * @return 会话目录路径；未配置会话时返回空路径。
     */
    [[nodiscard]] auto sessionPath() const -> std::filesystem::path {
        if (!m_sessionNaming.has_value()) {
            return {};
        }
        return buildSessionPath(*m_sessionNaming);
    }
    /**
     * @brief 配置会话命名并创建目录及 IFM 索引。
     * @param naming 会话命名信息；根目录会被替换为当前后端根目录。
     * @return 配置成功时为空；工作线程运行或文件创建失败时返回错误描述。
     */
    auto configureSession(ImageStorageNaming naming) -> std::expected<void, std::string> {
        if (m_running.load()) {
            return std::unexpected("cannot configure session while storage worker is running");
        }
        naming.rootPath = m_baseDir;
        std::error_code error;
        std::filesystem::create_directories(buildSessionPath(naming), error);
        if (error) {
            return std::unexpected("failed to create image session directory: " + error.message());
        }
        const auto indexPath = m_baseDir / buildIfmFileName(naming);
        std::ofstream index(indexPath, std::ios::trunc);
        if (!index) {
            return std::unexpected("failed to open image session index: " + indexPath.string());
        }
        index << buildIfmContent(naming);
        if (!index) {
            return std::unexpected("failed to write image session index: " + indexPath.string());
        }
        m_sessionNaming = std::move(naming);
        return {};
    }

    /**
     * @brief 按当前会话命名生成路径并提交一帧。
     * @param sequence 会话内帧序号。
     * @param metadata RAW 图像元数据。
     * @param pixels 16 位像素视图；函数返回前会复制数据。
     * @return 入队成功时为空；会话未配置或队列不可用时返回错误描述。
     */
    auto enqueueSessionFrame(std::uint64_t sequence, RawImageMetadata metadata,
                             std::span<const std::uint16_t> pixels)
        -> std::expected<void, std::string> {
        if (!m_sessionNaming.has_value()) {
            return std::unexpected("image storage session is not configured");
        }
        return enqueueRawFrame(buildImageFilePath(*m_sessionNaming, metadata, sequence),
                               std::move(metadata), pixels);
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

    /**
     * @brief 将相对路径解析到存储根目录。
     * @param path 相对或绝对目标路径。
     * @return 绝对路径保持不变；相对路径拼接到根目录。
     */
    [[nodiscard]] auto resolvePath(std::filesystem::path path) const -> std::filesystem::path {
        if (path.is_absolute()) {
            return path;
        }
        return m_baseDir / path;
    }

    /**
     * @brief 从队列取出请求并写入磁盘。
     * @param token 用于停止等待并在队列排空后退出的令牌。
     */
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
            const auto result = writeFrame(request);
            if (result) {
                m_successfulWrites.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_failedWrites.fetch_add(1, std::memory_order_relaxed);
                if (m_bus != nullptr) {
                    m_bus->emit(Dss::Core::StorageWriteErrorEvent{
                        .backend = "image_storage",
                        .path = request.path.string(),
                        .message = result.error(),
                    });
                }
            }
        }
    }

    /**
     * @brief 编码并写入一个 RAW 或兼容 BMP 帧。
     * @param request 包含目标路径、元数据及像素副本的写入请求。
     * @return 写入成功时为空；目录、编码或文件写入失败时返回错误描述。
     */
    auto writeFrame(const SaveRawFrameRequest& request) const -> std::expected<void, std::string> {
        std::error_code error;
        std::filesystem::create_directories(request.path.parent_path(), error);
        if (error) {
            return std::unexpected("failed to create storage directory: " + error.message());
        }

        std::vector<std::uint8_t> bytes;
        if (request.path.extension() == ".bmp") {
            LegacyBmpMetadata bmpMetadata{};
            bmpMetadata.width = request.metadata.width;
            bmpMetadata.height = request.metadata.height;
            bmpMetadata.timestamp = request.metadata.exposure.timestamp;
            bmpMetadata.azimuthDegrees = request.metadata.exposure.pointingAe.x;
            bmpMetadata.elevationDegrees = request.metadata.exposure.pointingAe.y;
            bmpMetadata.frameRate = request.metadata.frameFrequency;
            bmpMetadata.exposure = request.metadata.exposureTimeMilliseconds;
            bmpMetadata.temperature = request.metadata.exposure.temperature;
            bmpMetadata.humidity = request.metadata.exposure.humidity;
            bmpMetadata.atmosPressure = request.metadata.exposure.atmosPressure;
            const auto pixelBytes = std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(request.pixels.data()),
                request.pixels.size() * sizeof(std::uint16_t));
            auto encoded = buildLegacyBmpFile(bmpMetadata, pixelBytes);
            if (!encoded.has_value()) {
                return std::unexpected("failed to encode legacy bmp image");
            }
            bytes = std::move(*encoded);
        } else {
            bytes = buildRawImageFile(request.metadata, request.pixels);
        }

        std::ofstream output(request.path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return std::unexpected("failed to open storage file");
        }
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            return std::unexpected("failed to write storage file");
        }
        return {};
    }

    std::filesystem::path m_baseDir;                    ///< 存储根目录
    std::atomic<bool> m_ready{false};                   ///< 是否已完成初始化
    std::atomic<bool> m_running{false};                 ///< 后台线程运行状态
    std::size_t m_maxPendingRequests = 1024;            ///< 写入队列容量上限
    std::atomic<std::uint64_t> m_droppedRequests{0};    ///< 被背压拒绝的请求数量
    std::atomic<std::uint64_t> m_successfulWrites{0};   ///< 成功写入数量
    std::atomic<std::uint64_t> m_failedWrites{0};       ///< 写入失败数量
    std::optional<ImageStorageNaming> m_sessionNaming;  ///< 当前会话命名配置
    MessageBus* m_bus = nullptr;                        ///< 非拥有事件总线指针
    std::jthread m_worker;                              ///< 后台写入工作线程
    std::mutex m_queueMutex;                            ///< 保护写入队列的互斥锁
    std::condition_variable_any m_queueCv;              ///< 写入队列条件变量
    std::deque<SaveRawFrameRequest> m_queue;            ///< 待写入帧队列
};

}  // namespace Dss::Storage
