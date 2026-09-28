#include "dss/network/endpoint/image_sender.h"

#include <algorithm>
#include <limits>

#include "dss/core/concurrency/background_task.h"
#include "dss/core/event/events.h"

namespace Dss::Network {

namespace {

constexpr uint32_t ImagePacketMagic = 0xAAAA5555U;  ///< 图像分片魔数标识
constexpr uint8_t MonoChannelCount = 1U;            ///< 单通道灰度图像
constexpr uint8_t EightBitDepth = 8U;               ///< 8 位像素深度

/// 向缓冲区指定偏移写入大端序 16 位无符号整数
void writeU16Be(std::vector<uint8_t>& buffer, size_t offset, uint16_t value) {
    buffer[offset] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>(value & 0xFFU);
}

/// 向缓冲区指定偏移写入大端序 32 位无符号整数
void writeU32Be(std::vector<uint8_t>& buffer, size_t offset, uint32_t value) {
    buffer[offset] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    buffer[offset + 2U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 3U] = static_cast<uint8_t>(value & 0xFFU);
}

/// 向缓冲区指定偏移写入小端序 32 位无符号整数
void writeU32Le(std::vector<uint8_t>& buffer, size_t offset, uint32_t value) {
    buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

/**
 * @brief 为原始像素数据添加图像编码头（长度、宽高、通道数、位深）
 * @param imageData 原始像素数据
 * @param width 图像宽度（像素）
 * @param height 图像高度（像素）
 * @return 带 10 字节头部的完整编码缓冲区
 */
[[nodiscard]] auto buildEncodedImage(std::span<const uint8_t> imageData, uint32_t width,
                                     uint32_t height) -> std::vector<uint8_t> {
    std::vector<uint8_t> encoded(ImageSender::ImageHeaderSize + imageData.size());
    const auto encodedLength = static_cast<uint32_t>(encoded.size());
    writeU32Be(encoded, 0U, encodedLength);
    writeU16Be(encoded, 4U, static_cast<uint16_t>(width));
    writeU16Be(encoded, 6U, static_cast<uint16_t>(height));
    encoded[8U] = MonoChannelCount;
    encoded[9U] = EightBitDepth;
    std::copy(imageData.begin(), imageData.end(), encoded.begin() + ImageSender::ImageHeaderSize);
    return encoded;
}

}  // namespace

ImageSender::ImageSender(MessageBus& bus) : m_bus(bus) {}

ImageSender::~ImageSender() {
    close();
}

auto ImageSender::open(const UdpEndpointConfig& config) -> std::expected<void, std::string> {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
    auto result = m_channel.bind(config);
    if (!result) {
        return result;
    }

    {
        std::lock_guard lock(m_bufferMutex);
        m_accepting = true;
        m_hasPending = false;
        m_pendingImage.reset();
        m_pendingImageFactory = {};
    }
    m_workerThread = std::jthread([this](std::stop_token token) {
        Dss::Core::runBackgroundTask(m_bus, "image_sender", [this, token] { workerLoop(token); });
        std::lock_guard lock(m_bufferMutex);
        m_accepting = false;
        m_hasPending = false;
        m_pendingImage.reset();
        m_pendingImageFactory = {};
        m_resources.queuedItems = 0;
        m_resources.queuedBytes = 0;
        m_resources.activeItems = 0;
        m_resources.activeBytes = 0;
    });
    return {};
}

void ImageSender::close() {
    std::lock_guard lifecycleLock(m_lifecycleMutex);
    closeLocked();
}

void ImageSender::closeLocked() {
    const auto started = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(m_bufferMutex);
        m_resources.queuedItems = 0;
        m_resources.queuedBytes = 0;
        m_accepting = false;
        m_hasPending = false;
        m_pendingImage.reset();
        m_pendingImageFactory = {};
    }
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_bufferCv.notify_all();
        m_workerThread.join();
    }
    m_channel.close();
    std::lock_guard lock(m_bufferMutex);
    m_resources.queuedItems = 0;
    m_resources.queuedBytes = 0;
    m_resources.lastStopMicroseconds = Dss::Core::elapsedMicroseconds(started);
}

bool ImageSender::isOpen() const {
    return m_channel.isBound();
}

auto ImageSender::status() const -> Dss::Core::Status {
    return isOpen() ? Dss::Core::Status::Ok : Dss::Core::Status::Init;
}

void ImageSender::sendImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height) {
    auto sharedImage =
        std::make_shared<const std::vector<uint8_t>>(imageData.begin(), imageData.end());
    sendImage(0U, std::move(sharedImage), width, height);
}

void ImageSender::sendImage(uint64_t frameSeq,
                            std::shared_ptr<const std::vector<uint8_t>> imageData, uint32_t width,
                            uint32_t height) {
    if (!imageData || imageData->empty()) {
        return;
    }
    submitForSend(frameSeq, std::move(imageData), {}, width, height);
}

void ImageSender::submitForSend(uint64_t frameSeq,
                                std::shared_ptr<const std::vector<uint8_t>> image,
                                ImageFactory imageFactory, uint32_t width, uint32_t height,
                                std::size_t retainedSourceBytes) {
    if (!image && !imageFactory) {
        return;
    }
    {
        std::lock_guard lock(m_bufferMutex);
        if (!m_accepting) {
            return;
        }
        if (m_hasPending) {
            ++m_resources.replacedItems;
        }
        m_resources.queuedItems = 1;
        m_resources.queuedBytes =
            (image ? image->capacity() : 0) + (imageFactory ? retainedSourceBytes : 0);
        m_resources.peakQueuedBytes =
            (std::max)(m_resources.peakQueuedBytes, m_resources.queuedBytes);
        m_pendingImage = std::move(image);
        m_pendingImageFactory = std::move(imageFactory);
        m_pendingFrameSeq = frameSeq;
        m_pendingWidth = width;
        m_pendingHeight = height;
        m_hasPending = true;
    }
    m_bufferCv.notify_one();
}

auto ImageSender::resourceSnapshot() const -> Dss::Core::ResourceSnapshot {
    std::lock_guard lock(m_bufferMutex);
    return m_resources;
}

auto ImageSender::buildPackets(std::span<const uint8_t> imageData, uint32_t width, uint32_t height)
    -> std::vector<std::vector<uint8_t>> {
    if (imageData.empty() ||
        imageData.size() > std::numeric_limits<uint32_t>::max() - ImageHeaderSize) {
        return {};
    }

    const auto encoded = buildEncodedImage(imageData, width, height);
    const auto totalPackets = (encoded.size() + MaxUdpPayload - 1U) / MaxUdpPayload;
    if (totalPackets > std::numeric_limits<uint32_t>::max()) {
        return {};
    }

    std::vector<std::vector<uint8_t>> packets;
    packets.reserve(totalPackets);

    uint32_t sequence = 1U;
    for (size_t offset = 0U; offset < encoded.size(); offset += MaxUdpPayload) {
        const auto chunkSize = std::min(MaxUdpPayload, encoded.size() - offset);
        std::vector<uint8_t> packet(PacketHeaderSize + chunkSize + PacketPaddingSize);
        writeU32Le(packet, 0U, ImagePacketMagic);
        writeU32Le(packet, 4U, static_cast<uint32_t>(totalPackets));
        writeU32Le(packet, 8U, static_cast<uint32_t>(encoded.size()));
        writeU32Le(packet, 12U, sequence);
        writeU32Le(packet, 16U, static_cast<uint32_t>(chunkSize));
        std::copy_n(encoded.begin() + static_cast<std::ptrdiff_t>(offset), chunkSize,
                    packet.begin() + PacketHeaderSize);
        packets.push_back(std::move(packet));
        ++sequence;
    }

    return packets;
}

void ImageSender::workerLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        std::shared_ptr<const std::vector<uint8_t>> image;
        ImageFactory factory;
        uint64_t frameSeq = 0;
        uint32_t w = 0;
        uint32_t h = 0;

        {
            std::unique_lock lock(m_bufferMutex);
            m_bufferCv.wait(lock, token, [this]() { return m_hasPending; });

            if (token.stop_requested()) {
                break;
            }

            image = std::move(m_pendingImage);
            factory = std::move(m_pendingImageFactory);
            frameSeq = m_pendingFrameSeq;
            w = m_pendingWidth;
            h = m_pendingHeight;
            m_hasPending = false;
            m_resources.activeItems = 1;
            m_resources.activeBytes = m_resources.queuedBytes;
            m_resources.queuedItems = 0;
            m_resources.queuedBytes = 0;
        }
        const auto started = std::chrono::steady_clock::now();
        const auto finishWork = [this, started] {
            std::lock_guard lock(m_bufferMutex);
            m_resources.activeItems = 0;
            m_resources.activeBytes = 0;
            ++m_resources.completedItems;
            m_resources.lastWorkMicroseconds = Dss::Core::elapsedMicroseconds(started);
            m_resources.maxWorkMicroseconds =
                (std::max)(m_resources.maxWorkMicroseconds, m_resources.lastWorkMicroseconds);
        };

        // image 为空时由 factory 在本工作线程生成,把整图拉伸移出 ImageProcessor 处理线程
        if ((!image || image->empty()) && factory) {
            image = factory();
        }
        factory = {};
        if (!image || image->empty()) {
            finishWork();
            continue;
        }

        auto packets = buildPackets(*image, w, h);
        {
            std::lock_guard lock(m_bufferMutex);
            m_resources.activeBytes = image->capacity();
            for (const auto& packet : packets) {
                m_resources.activeBytes += packet.capacity();
            }
        }
        bool sentAllPackets = !packets.empty();
        for (const auto& packet : packets) {
            if (m_channel.send(packet) != static_cast<int64_t>(packet.size())) {
                sentAllPackets = false;
                m_bus.emit(Dss::Core::NetworkTransmissionErrorEvent{
                    .channel = "image_sender",
                    .message = "failed to send image UDP fragment",
                    .attemptedBytes = static_cast<uint64_t>(packet.size()),
                });
                break;
            }
        }

        if (sentAllPackets) {
            m_bus.emit(Dss::Core::ImageSendCompletedEvent{frameSeq});
        }
        packets.clear();
        image.reset();
        finishWork();
    }
}

}  // namespace Dss::Network
