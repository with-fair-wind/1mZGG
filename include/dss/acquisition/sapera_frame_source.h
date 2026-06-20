#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>

#include "dss/acquisition/i_frame_source.h"
#include "dss/core/event_bus.h"

namespace Dss::Acquisition {

struct SaperaFrameGeometry {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct SaperaConfig {
    std::string serverName{"Xtium-CL_MX4_1"};
    int32_t deviceNumber = 0;
    std::string ccfPath;
};

class ISaperaCaptureSession {
public:
    using FrameCallback = std::function<void(std::span<const uint16_t>)>;

    virtual ~ISaperaCaptureSession() = default;
    virtual auto initialize(FrameCallback callback)
        -> std::expected<SaperaFrameGeometry, std::string> = 0;
    virtual auto start() -> std::expected<void, std::string> = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
};

class SaperaFrameSource final : public IFrameSource {
public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;

    explicit SaperaFrameSource(SaperaConfig config = {},
                               std::unique_ptr<ISaperaCaptureSession> session = {},
                               MessageBus* bus = nullptr);
    ~SaperaFrameSource() override;

    SaperaFrameSource(const SaperaFrameSource&) = delete;
    SaperaFrameSource& operator=(const SaperaFrameSource&) = delete;

    auto init() -> std::expected<void, std::string> override;
    auto startCapture() -> std::expected<void, std::string>;
    void start() override;
    void stop() override;
    void setFrameCallback(FrameCallback callback) override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] auto frameWidth() const -> uint32_t override;
    [[nodiscard]] auto frameHeight() const -> uint32_t override;

private:
    void acceptFrame(std::span<const uint16_t> pixels);
    void reportError(const std::string& message) const;

    SaperaConfig m_config;
    std::unique_ptr<ISaperaCaptureSession> m_session;
    MessageBus* m_bus = nullptr;

    mutable std::mutex m_mutex;
    FrameCallback m_callback;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint64_t m_frameSeq = 0;
    bool m_initialized = false;
};

}  // namespace Dss::Acquisition