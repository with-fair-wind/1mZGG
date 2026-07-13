#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "dss/core/event/events.h"
#include "dss/core/event/message_bus.h"
#include "dss/storage/backend/i_storage_backend.h"
#include "dss/storage/detail/async_write_queue.h"
#include "dss/storage/format/bmp_image_format.h"
#include "dss/storage/format/image_storage_format.h"

namespace Dss::Storage {

/// 本地 RAW 图像异步存储后端，通过后台线程写入磁盘
class LocalImageStorageBackend final : public IStorageBackend {
public:
    using MessageBus = Dss::Core::MessageBus;  ///< 跨线程消息总线类型
    /**
     * @brief 构造本地图像存储后端。
     * @param baseDir 默认存储根目录。
     * @param maxPendingRequests 写入队列允许的最大待处理请求数。
     */
    explicit LocalImageStorageBackend(std::filesystem::path baseDir,
                                      std::size_t maxPendingRequests = 1024)
        : m_baseDir(std::move(baseDir)), m_writeQueue(maxPendingRequests) {}

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
        if (m_writeQueue.isRunning()) {
            return {};
        }

        return m_writeQueue.start(
            [this](const SaveRawFrameRequest& request) { return writeFrame(request); },
            [this](const SaveRawFrameRequest& request, const std::string& message) {
                if (m_bus != nullptr) {
                    m_bus->emit(Dss::Core::StorageWriteErrorEvent{
                        .backend = "image_storage",
                        .path = request.path.string(),
                        .message = message,
                    });
                }
            });
    }

    /// 停止后台写入工作线程并等待其退出
    void stop() {
        m_writeQueue.stop();
    }

    /** @brief 查询后台写入状态。 @return 工作线程运行时返回 true。 */
    [[nodiscard]] bool isRunning() const {
        return m_writeQueue.isRunning();
    }

    /**
     * @brief 获取累计丢弃请求数。
     * @return 因写入队列已满而拒绝的请求数量。
     */
    [[nodiscard]] auto droppedRequests() const -> std::uint64_t {
        return m_writeQueue.droppedRequests();
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
        return m_writeQueue.successfulWrites();
    }

    /** @brief 获取失败写入次数。 @return 后台写入失败的帧数量。 */
    [[nodiscard]] auto failedWrites() const -> std::uint64_t {
        return m_writeQueue.failedWrites();
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
        if (isRunning()) {
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
        return enqueueSessionFrame(
            sequence, std::move(metadata),
            std::make_shared<const std::vector<std::uint16_t>>(pixels.begin(), pixels.end()));
    }

    /**
     * @brief 按当前会话命名提交一份不可变共享 RAW 帧。
     * @param sequence 会话内帧序号。
     * @param metadata RAW 图像元数据。
     * @param pixels 由写入请求共享持有的 16 位像素缓冲。
     * @return 入队成功时为空；缓冲无效、会话未配置或队列不可用时返回错误描述。
     */
    auto enqueueSessionFrame(std::uint64_t sequence, RawImageMetadata metadata,
                             std::shared_ptr<const std::vector<std::uint16_t>> pixels)
        -> std::expected<void, std::string> {
        if (!m_sessionNaming.has_value()) {
            return std::unexpected("image storage session is not configured");
        }
        return enqueueRawFrame(buildImageFilePath(*m_sessionNaming, metadata, sequence),
                               std::move(metadata), std::move(pixels));
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
        return enqueueRawFrame(
            std::move(relativePath), std::move(metadata),
            std::make_shared<const std::vector<std::uint16_t>>(pixels.begin(), pixels.end()));
    }

    /**
     * @brief 将不可变共享 RAW 帧加入异步写入队列。
     * @param relativePath 相对或绝对文件路径。
     * @param metadata 帧元数据。
     * @param pixels 由写入请求共享持有的 16 位像素缓冲。
     * @return 成功时返回空值；缓冲无效、未初始化或未运行时返回错误描述。
     */
    auto enqueueRawFrame(std::filesystem::path relativePath, RawImageMetadata metadata,
                         std::shared_ptr<const std::vector<std::uint16_t>> pixels)
        -> std::expected<void, std::string> {
        if (!m_ready.load()) {
            return std::unexpected("storage backend is not initialized");
        }
        if (!isRunning()) {
            return std::unexpected("storage worker is not running");
        }
        if (!pixels || pixels->empty()) {
            return std::unexpected("raw image buffer is empty");
        }

        SaveRawFrameRequest request{};
        request.path = resolvePath(std::move(relativePath));
        request.metadata = std::move(metadata);
        request.pixels = std::move(pixels);
        auto result = m_writeQueue.enqueue(std::move(request));
        if (!result.has_value() && result.error() == "async write queue is full") {
            return std::unexpected("storage queue is full");
        }
        return result;
    }

private:
    /// 待写入的 RAW 帧请求
    struct SaveRawFrameRequest {
        std::filesystem::path path;                                ///< 目标文件路径
        RawImageMetadata metadata{};                               ///< 帧元数据
        std::shared_ptr<const std::vector<std::uint16_t>> pixels;  ///< 共享 16 位像素数据
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
                reinterpret_cast<const std::uint8_t*>(request.pixels->data()),
                request.pixels->size() * sizeof(std::uint16_t));
            auto encoded = buildLegacyBmpFile(bmpMetadata, pixelBytes);
            if (!encoded.has_value()) {
                return std::unexpected("failed to encode legacy bmp image");
            }
            bytes = std::move(*encoded);
        } else {
            bytes = buildRawImageFile(request.metadata, *request.pixels);
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
    std::optional<ImageStorageNaming> m_sessionNaming;  ///< 当前会话命名配置
    MessageBus* m_bus = nullptr;                        ///< 非拥有事件总线指针
    AsyncWriteQueue<SaveRawFrameRequest> m_writeQueue;  ///< 后台写入队列
};

}  // namespace Dss::Storage
