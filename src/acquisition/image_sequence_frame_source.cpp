#include "dss/acquisition/image_sequence_frame_source.h"

#include <QImage>
#include <QString>
#include <cwctype>
#include <fstream>
#include <limits>
#include <span>
#include <utility>

#include "dss/storage/bmp_image_format.h"
#include "dss/storage/image_storage_format.h"

namespace Dss::Acquisition {
namespace {

[[nodiscard]] auto lowerExtension(const std::filesystem::path& path) -> std::wstring {
    auto ext = path.extension().wstring();
    for (auto& ch : ext) {
        ch = static_cast<wchar_t>(::towlower(ch));
    }
    return ext;
}

[[nodiscard]] auto isRawFile(const std::filesystem::path& path) -> bool {
    return lowerExtension(path) == L".raw";
}

[[nodiscard]] auto isBmpFile(const std::filesystem::path& path) -> bool {
    return lowerExtension(path) == L".bmp";
}

/// @brief 读取完整二进制文件
/// @param path 文件路径
/// @return 文件字节流；打开失败时返回错误
[[nodiscard]] auto readFileBytes(const std::filesystem::path& path)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return std::unexpected("failed to open image file: " + path.string());
    }

    const auto endPosition = input.tellg();
    if (endPosition < std::streampos{0}) {
        return std::unexpected("failed to query image file size: " + path.string());
    }

    const auto byteCount = static_cast<std::uintmax_t>(endPosition);
    if (byteCount > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        byteCount > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return std::unexpected("image file is too large: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(byteCount));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            return std::unexpected("failed to read image file: " + path.string());
        }
    }
    return bytes;
}

/**
 * @brief 从 .raw 文件加载一帧
 * @param path RAW 图像文件路径
 * @param frameSeq 帧序号
 * @return 解码后的帧数据包；打开或解码失败时返回错误
 */
[[nodiscard]] auto loadRawFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    auto bytes = readFileBytes(path);
    if (!bytes.has_value()) {
        return std::unexpected(bytes.error());
    }

    auto decoded = Dss::Storage::decodeRawImageFile(std::span<const std::uint8_t>(*bytes));
    if (!decoded.has_value()) {
        return std::unexpected("failed to decode raw image file: " + path.string());
    }

    Dss::Processing::FramePacket packet{};
    packet.frameSeq = frameSeq;
    packet.width = decoded->metadata.width;
    packet.height = decoded->metadata.height;
    packet.metadata = decoded->metadata.exposure;
    packet.metadata.exposureTime =
        static_cast<float>(decoded->metadata.exposureTimeMilliseconds / 1000.0);
    packet.metadata.frameFrequency = static_cast<float>(decoded->metadata.frameFrequency);
    packet.rawImage = std::move(decoded->pixels);
    return packet;
}

/// @brief 从 oldsrc/ImageCode 兼容的遗留 BMP 文件加载一帧
/// @param path BMP 图像文件路径
/// @param frameSeq 帧序号
/// @return 解码后的帧数据包；非遗留 BMP 或解码失败时返回错误
[[nodiscard]] auto loadLegacyBmpFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    auto bytes = readFileBytes(path);
    if (!bytes.has_value()) {
        return std::unexpected(bytes.error());
    }

    auto decoded = Dss::Storage::decodeLegacyBmpFile(std::span<const std::uint8_t>(*bytes));
    if (!decoded.has_value()) {
        return std::unexpected("failed to decode legacy bmp image file: " + path.string());
    }

    Dss::Processing::FramePacket packet{};
    packet.frameSeq = frameSeq;
    packet.width = decoded->metadata.width;
    packet.height = decoded->metadata.height;
    packet.metadata.timestamp = decoded->metadata.timestamp;
    packet.metadata.pointingAe = {.x = static_cast<float>(decoded->metadata.azimuthDegrees),
                                  .y = static_cast<float>(decoded->metadata.elevationDegrees)};
    packet.metadata.exposureTime = static_cast<float>(decoded->metadata.exposure / 1000.0);
    packet.metadata.frameFrequency = static_cast<float>(decoded->metadata.frameRate);
    packet.metadata.temperature = decoded->metadata.temperature;
    packet.metadata.atmosPressure = decoded->metadata.atmosPressure;
    packet.metadata.humidity = decoded->metadata.humidity;
    packet.rawImage = std::move(decoded->pixels);
    return packet;
}

/**
 * @brief 从通用图像文件（PNG/JPG 等）加载一帧
 * @param path 图像文件路径
 * @param frameSeq 帧序号
 * @return 转换后的帧数据包；加载失败时返回错误
 */
[[nodiscard]] auto loadImageFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    QImage image(QString::fromStdWString(path.wstring()));
    if (image.isNull()) {
        return std::unexpected("failed to load image file: " + path.string());
    }

    auto gray = image.convertToFormat(QImage::Format_Grayscale8);
    Dss::Processing::FramePacket packet{};
    packet.frameSeq = frameSeq;
    packet.width = static_cast<std::uint32_t>(gray.width());
    packet.height = static_cast<std::uint32_t>(gray.height());

    const auto width = static_cast<std::size_t>(gray.width());
    const auto height = static_cast<std::size_t>(gray.height());
    packet.displayImage.resize(width * height);
    packet.rawImage.resize(width * height);
    for (std::size_t y = 0; y < height; ++y) {
        const auto* src = gray.constScanLine(static_cast<int>(y));
        auto* displayRow = packet.displayImage.data() + y * width;
        auto* rawRow = packet.rawImage.data() + y * width;
        for (std::size_t x = 0; x < width; ++x) {
            displayRow[x] = src[x];
            rawRow[x] = static_cast<std::uint16_t>(static_cast<std::uint16_t>(src[x]) << 8U);
        }
    }
    return packet;
}

/**
 * @brief 根据文件扩展名选择加载方式
 * @param path 图像文件路径
 * @param frameSeq 帧序号
 * @return 帧数据包；加载失败时返回错误
 */
[[nodiscard]] auto loadFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    if (isRawFile(path)) {
        return loadRawFrame(path, frameSeq);
    }
    if (isBmpFile(path)) {
        if (auto legacyBmp = loadLegacyBmpFrame(path, frameSeq); legacyBmp.has_value()) {
            return legacyBmp;
        }
    }
    return loadImageFrame(path, frameSeq);
}

}  // namespace

ImageSequenceFrameSource::ImageSequenceFrameSource() = default;

ImageSequenceFrameSource::ImageSequenceFrameSource(std::vector<std::filesystem::path> files)
    : m_files(std::move(files)) {}

ImageSequenceFrameSource::~ImageSequenceFrameSource() {
    stop();
}

/**
 * @brief 替换文件列表并重置播放状态
 * @param files 新的图像文件路径序列
 * @return 列表为空时返回错误
 */
auto ImageSequenceFrameSource::setFiles(std::vector<std::filesystem::path> files)
    -> std::expected<void, std::string> {
    stop();
    {
        std::lock_guard lock(m_mutex);
        m_files = std::move(files);
        m_width = 0;
        m_height = 0;
        m_nextFrameIndex = 0;
        m_initialized = false;
    }
    if (frameCount() == 0U) {
        return std::unexpected("image sequence is empty");
    }
    return {};
}

auto ImageSequenceFrameSource::frameCount() const -> std::size_t {
    std::lock_guard lock(m_mutex);
    return m_files.size();
}

auto ImageSequenceFrameSource::nextFrameIndex() const -> std::size_t {
    std::lock_guard lock(m_mutex);
    return m_nextFrameIndex;
}

auto ImageSequenceFrameSource::seek(std::size_t index) -> std::expected<void, std::string> {
    {
        std::lock_guard lock(m_mutex);
        if (index >= m_files.size()) {
            return std::unexpected("replay frame index is out of range");
        }
    }
    const auto resume = isRunning();
    if (resume) {
        stop();
    }

    {
        std::lock_guard lock(m_mutex);
        m_nextFrameIndex = index;
    }

    if (resume) {
        start();
    }
    return {};
}

void ImageSequenceFrameSource::setFrameInterval(std::chrono::milliseconds interval) {
    std::lock_guard lock(m_mutex);
    m_frameInterval = interval;
}

/**
 * @brief 同步加载当前索引帧并触发回调，随后推进索引
 * @return 序列为空、未设置回调、已到末尾或加载失败时返回错误
 */
auto ImageSequenceFrameSource::stepForward() -> std::expected<void, std::string> {
    std::filesystem::path file;
    std::size_t index = 0;
    FrameCallback callback;
    {
        std::lock_guard lock(m_mutex);
        if (m_files.empty()) {
            return std::unexpected("image sequence is empty");
        }
        if (!m_callback) {
            return std::unexpected("frame callback is not set");
        }
        if (m_nextFrameIndex >= m_files.size()) {
            return std::unexpected("image sequence is at the end");
        }
        index = m_nextFrameIndex;
        file = m_files[index];
        callback = m_callback;
    }

    auto packet = loadFrame(file, static_cast<std::uint64_t>(index));
    if (!packet.has_value()) {
        return std::unexpected(packet.error());
    }

    callback(std::move(*packet));
    {
        std::lock_guard lock(m_mutex);
        if (m_nextFrameIndex == index) {
            ++m_nextFrameIndex;
        }
    }
    return {};
}

/**
 * @brief 加载首帧以确定帧尺寸并重置播放索引
 * @return 序列为空或首帧加载失败时返回错误
 */
auto ImageSequenceFrameSource::init() -> std::expected<void, std::string> {
    std::vector<std::filesystem::path> files;
    {
        std::lock_guard lock(m_mutex);
        files = m_files;
    }
    if (files.empty()) {
        return std::unexpected("image sequence is empty");
    }

    auto packet = loadFrame(files.front(), 0);
    if (!packet.has_value()) {
        return std::unexpected(packet.error());
    }

    {
        std::lock_guard lock(m_mutex);
        m_width = packet->width;
        m_height = packet->height;
        m_nextFrameIndex = 0;
        m_initialized = true;
    }
    return {};
}

/**
 * @brief 在后台线程中从当前索引连续回放至序列末尾
 */
void ImageSequenceFrameSource::start() {
    if (m_running.exchange(true)) {
        return;
    }

    std::vector<std::filesystem::path> files;
    std::chrono::milliseconds interval{0};
    FrameCallback callback;
    std::size_t startIndex = 0;
    {
        std::lock_guard lock(m_mutex);
        files = m_files;
        interval = m_frameInterval;
        callback = m_callback;
        startIndex = m_nextFrameIndex;
    }

    if (files.empty() || !callback || startIndex >= files.size()) {
        m_running.store(false);
        return;
    }

    m_worker = std::jthread([this, files = std::move(files), interval, startIndex,
                             callback = std::move(callback)](std::stop_token token) mutable {
        for (std::size_t index = startIndex; index < files.size(); ++index) {
            if (token.stop_requested()) {
                break;
            }

            auto packet = loadFrame(files[index], static_cast<std::uint64_t>(index));
            if (packet.has_value()) {
                callback(std::move(*packet));
                std::lock_guard lock(m_mutex);
                if (m_nextFrameIndex <= index) {
                    m_nextFrameIndex = index + 1U;
                }
            }

            if (interval.count() > 0 && index + 1U < files.size()) {
                std::this_thread::sleep_for(interval);
            }
        }
        m_running.store(false);
    });
}

void ImageSequenceFrameSource::stop() {
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    m_running.store(false);
}

void ImageSequenceFrameSource::setFrameCallback(FrameCallback callback) {
    std::lock_guard lock(m_mutex);
    m_callback = std::move(callback);
}

bool ImageSequenceFrameSource::isRunning() const {
    return m_running.load();
}

auto ImageSequenceFrameSource::frameWidth() const -> std::uint32_t {
    std::lock_guard lock(m_mutex);
    return m_width;
}

auto ImageSequenceFrameSource::frameHeight() const -> std::uint32_t {
    std::lock_guard lock(m_mutex);
    return m_height;
}

}  // namespace Dss::Acquisition
