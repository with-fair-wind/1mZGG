#include "dss/acquisition/image_sequence_frame_source.h"

#include <QImage>
#include <QString>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <span>
#include <utility>

#include "dss/storage/image_storage_format.h"

namespace Dss::Acquisition {
namespace {

[[nodiscard]] auto rawToDisplay(std::span<const std::uint16_t> raw) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> display;
    display.reserve(raw.size());
    for (const auto pixel : raw) {
        display.push_back(static_cast<std::uint8_t>(pixel >> 8U));
    }
    return display;
}

[[nodiscard]] auto isRawFile(const std::filesystem::path& path) -> bool {
    auto ext = path.extension().wstring();
    for (auto& ch : ext) {
        ch = static_cast<wchar_t>(::towlower(ch));
    }
    return ext == L".raw";
}

[[nodiscard]] auto loadRawFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("failed to open raw image file: " + path.string());
    }

    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    auto decoded = Dss::Storage::decodeRawImageFile(std::span<const std::uint8_t>(bytes));
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
    packet.displayImage = rawToDisplay(packet.rawImage);
    return packet;
}

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

[[nodiscard]] auto loadFrame(const std::filesystem::path& path, std::uint64_t frameSeq)
    -> std::expected<Dss::Processing::FramePacket, std::string> {
    if (isRawFile(path)) {
        return loadRawFrame(path, frameSeq);
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

auto ImageSequenceFrameSource::setFiles(std::vector<std::filesystem::path> files)
    -> std::expected<void, std::string> {
    stop();
    {
        std::lock_guard lock(m_mutex);
        m_files = std::move(files);
        m_width = 0;
        m_height = 0;
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

void ImageSequenceFrameSource::setFrameInterval(std::chrono::milliseconds interval) {
    std::lock_guard lock(m_mutex);
    m_frameInterval = interval;
}

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
        m_initialized = true;
    }
    return {};
}

void ImageSequenceFrameSource::start() {
    if (m_running.exchange(true)) {
        return;
    }

    std::vector<std::filesystem::path> files;
    std::chrono::milliseconds interval{0};
    FrameCallback callback;
    {
        std::lock_guard lock(m_mutex);
        files = m_files;
        interval = m_frameInterval;
        callback = m_callback;
    }

    if (files.empty() || !callback) {
        m_running.store(false);
        return;
    }

    m_worker = std::jthread([this, files = std::move(files), interval,
                             callback = std::move(callback)](std::stop_token token) mutable {
        for (std::size_t index = 0; index < files.size(); ++index) {
            if (token.stop_requested()) {
                break;
            }

            auto packet = loadFrame(files[index], static_cast<std::uint64_t>(index));
            if (packet.has_value()) {
                callback(std::move(*packet));
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
