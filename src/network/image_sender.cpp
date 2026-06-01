#include "dss/network/image_sender.h"

#include "dss/core/events.h"

#include <algorithm>
#include <limits>

namespace Dss::Network
{

namespace
{

constexpr uint32_t ImagePacketMagic = 0xAAAA5555U;
constexpr uint8_t MonoChannelCount = 1U;
constexpr uint8_t EightBitDepth = 8U;

void writeU16Be(std::vector<uint8_t>& buffer, size_t offset, uint16_t value)
{
    buffer[offset] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>(value & 0xFFU);
}

void writeU32Be(std::vector<uint8_t>& buffer, size_t offset, uint32_t value)
{
    buffer[offset] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    buffer[offset + 2U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 3U] = static_cast<uint8_t>(value & 0xFFU);
}

void writeU32Le(std::vector<uint8_t>& buffer, size_t offset, uint32_t value)
{
    buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
    buffer[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] auto buildEncodedImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height)
    -> std::vector<uint8_t>
{
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

} // namespace

ImageSender::ImageSender(MessageBus& bus) : m_bus(bus) {}

ImageSender::~ImageSender()
{
    close();
}

auto ImageSender::open(const UdpEndpointConfig& config) -> std::expected<void, std::string>
{
    auto result = m_channel.bind(config);
    if (!result)
    {
        return result;
    }

    m_workerThread = std::jthread([this](std::stop_token token) { workerLoop(token); });
    return {};
}

void ImageSender::close()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_bufferCv.notify_all();
        m_workerThread.join();
    }
    m_channel.close();
}

void ImageSender::sendImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height)
{
    {
        std::lock_guard lock(m_bufferMutex);
        m_pendingImage.assign(imageData.begin(), imageData.end());
        m_pendingWidth = width;
        m_pendingHeight = height;
        m_hasPending = true;
    }
    m_bufferCv.notify_one();
}

auto ImageSender::buildPackets(std::span<const uint8_t> imageData, uint32_t width, uint32_t height)
    -> std::vector<std::vector<uint8_t>>
{
    if (imageData.empty() || imageData.size() > std::numeric_limits<uint32_t>::max() - ImageHeaderSize)
    {
        return {};
    }

    const auto encoded = buildEncodedImage(imageData, width, height);
    const auto totalPackets = (encoded.size() + MaxUdpPayload - 1U) / MaxUdpPayload;
    if (totalPackets > std::numeric_limits<uint32_t>::max())
    {
        return {};
    }

    std::vector<std::vector<uint8_t>> packets;
    packets.reserve(totalPackets);

    uint32_t sequence = 1U;
    for (size_t offset = 0U; offset < encoded.size(); offset += MaxUdpPayload)
    {
        const auto chunkSize = std::min(MaxUdpPayload, encoded.size() - offset);
        std::vector<uint8_t> packet(PacketHeaderSize + chunkSize + PacketPaddingSize);
        writeU32Le(packet, 0U, ImagePacketMagic);
        writeU32Le(packet, 4U, static_cast<uint32_t>(totalPackets));
        writeU32Le(packet, 8U, static_cast<uint32_t>(encoded.size()));
        writeU32Le(packet, 12U, sequence);
        writeU32Le(packet, 16U, static_cast<uint32_t>(chunkSize));
        std::copy_n(
            encoded.begin() + static_cast<std::ptrdiff_t>(offset), chunkSize, packet.begin() + PacketHeaderSize);
        packets.push_back(std::move(packet));
        ++sequence;
    }

    return packets;
}

void ImageSender::workerLoop(std::stop_token token)
{
    while (!token.stop_requested())
    {
        std::vector<uint8_t> image;
        uint32_t w = 0;
        uint32_t h = 0;

        {
            std::unique_lock lock(m_bufferMutex);
            m_bufferCv.wait(lock, token, [this]() { return m_hasPending; });

            if (token.stop_requested())
            {
                break;
            }

            image = std::move(m_pendingImage);
            w = m_pendingWidth;
            h = m_pendingHeight;
            m_hasPending = false;
        }

        if (image.empty())
        {
            continue;
        }

        const auto packets = buildPackets(image, w, h);
        for (const auto& packet : packets)
        {
            m_channel.send(packet);
        }

        m_bus.emit(Dss::Core::ImageSendEvent{0});
    }
}

} // namespace Dss::Network
