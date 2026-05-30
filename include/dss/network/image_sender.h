#pragma once

#include "dss/core/event_bus.h"
#include "dss/network/udp_channel.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

namespace Dss::Network
{

class ImageSender
{
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    static constexpr size_t MaxUdpPayload = 60000;

    explicit ImageSender(MessageBus& bus);
    ~ImageSender();

    auto open(const UdpEndpointConfig& config) -> std::expected<void, std::string>;
    void close();

    void sendImage(std::span<const uint8_t> imageData, uint32_t width, uint32_t height);

private:
    void workerLoop(std::stop_token token);

    MessageBus& m_bus;
    UdpChannel m_channel;
    std::jthread m_workerThread;

    std::mutex m_bufferMutex;
    std::condition_variable_any m_bufferCv;
    std::vector<uint8_t> m_pendingImage;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;
    bool m_hasPending = false;
};

} // namespace Dss::Network
