#include "dss/network/image_sender.h"

#include "dss/core/events.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace Dss::Network
{

ImageSender::ImageSender(MessageBus& bus)
    : m_bus(bus)
{
}

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

        // 10-byte header: totalLen(4) + width(2) + height(2) + channels(1) + depth(1)
        std::array<uint8_t, 10> header{};
        auto totalLen = static_cast<uint32_t>(image.size());
        std::memcpy(header.data(), &totalLen, 4);
        auto w16 = static_cast<uint16_t>(w);
        auto h16 = static_cast<uint16_t>(h);
        std::memcpy(header.data() + 4, &w16, 2);
        std::memcpy(header.data() + 6, &h16, 2);
        header[8] = 1; // channels
        header[9] = 8; // bit depth

        // Fragment and send
        size_t offset = 0;
        uint16_t seq = 0;
        while (offset < image.size())
        {
            size_t chunk = std::min(MaxUdpPayload - 20, image.size() - offset);

            std::vector<uint8_t> packet(20 + chunk);
            // Packet header: magic(4) + seq(2) + totalPackets(2) + offset(4) + chunkLen(4) + header(4)
            uint32_t magic = 0xAAAA5555;
            std::memcpy(packet.data(), &magic, 4);
            std::memcpy(packet.data() + 4, &seq, 2);
            auto totalPackets =
                static_cast<uint16_t>((image.size() + MaxUdpPayload - 21) / (MaxUdpPayload - 20));
            std::memcpy(packet.data() + 6, &totalPackets, 2);
            auto off32 = static_cast<uint32_t>(offset);
            std::memcpy(packet.data() + 8, &off32, 4);
            auto chunk32 = static_cast<uint32_t>(chunk);
            std::memcpy(packet.data() + 12, &chunk32, 4);
            std::memcpy(packet.data() + 16, header.data(), 4);
            std::memcpy(packet.data() + 20, image.data() + offset, chunk);

            m_channel.send(packet);

            offset += chunk;
            ++seq;
        }

        m_bus.emit(Dss::Core::ImageSendEvent{0});
    }
}

} // namespace Dss::Network
