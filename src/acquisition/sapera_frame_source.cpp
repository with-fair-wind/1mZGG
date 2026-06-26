#include "dss/acquisition/sapera_frame_source.h"

#include <limits>
#include <utility>
#include <vector>

#include "dss/core/events.h"

#ifdef DSS_HAS_SAPERA
#include <SapClassBasic.h>
#endif

namespace Dss::Acquisition {
namespace {

#ifdef DSS_HAS_SAPERA
class SaperaSdkCaptureSession final : public ISaperaCaptureSession {
public:
    explicit SaperaSdkCaptureSession(SaperaConfig config) : m_config(std::move(config)) {}

    ~SaperaSdkCaptureSession() override {
        stop();
        destroy();
    }

    auto initialize(FrameCallback callback)
        -> std::expected<SaperaFrameGeometry, std::string> override {
        destroy();
        if (m_config.ccfPath.empty()) {
            return std::unexpected("Sapera CCF path is empty");
        }
        if (SapManager::GetResourceCount(m_config.serverName.c_str(), SapManager::ResourceAcq) <= 0) {
            return std::unexpected("Sapera acquisition device is unavailable");
        }

        const SapLocation location(m_config.serverName.c_str(), m_config.deviceNumber);
        m_acquisition =
            std::make_unique<SapAcquisition>(location, m_config.ccfPath.c_str());
        m_buffers = std::make_unique<SapBufferWithTrash>(1, m_acquisition.get());
        m_transfer = std::make_unique<SapAcqToBuf>(
            m_acquisition.get(), m_buffers.get(), &SaperaSdkCaptureSession::transferCallback, this);

        if (!m_acquisition->Create() || !m_buffers->Create() || !m_transfer->Create()) {
            destroy();
            return std::unexpected("Sapera objects could not be created");
        }

        int width = 0;
        int height = 0;
        int pixelDepth = 0;
        if (!m_acquisition->GetParameter(CORACQ_PRM_CROP_WIDTH, &width) ||
            !m_acquisition->GetParameter(CORACQ_PRM_CROP_HEIGHT, &height) ||
            !m_acquisition->GetParameter(CORACQ_PRM_PIXEL_DEPTH, &pixelDepth) ||
            width <= 0 || height <= 0 || pixelDepth != 16) {
            destroy();
            return std::unexpected("Sapera source must provide a valid 16-bit frame");
        }

        void* address = nullptr;
        if (!m_buffers->GetParameter(CORBUFFER_PRM_ADDRESS, &address) || address == nullptr) {
            destroy();
            return std::unexpected("Sapera image buffer is unavailable");
        }

        m_callback = std::move(callback);
        m_pixels = static_cast<const uint16_t*>(address);
        m_pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        m_geometry = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        return m_geometry;
    }

    auto start() -> std::expected<void, std::string> override {
        if (!m_transfer || !m_transfer->Grab()) {
            return std::unexpected("Sapera transfer start failed");
        }
        m_running = true;
        return {};
    }

    void stop() override {
        if (m_running && m_transfer) {
            (void)m_transfer->Freeze();
        }
        m_running = false;
    }

    [[nodiscard]] bool isRunning() const override {
        return m_running;
    }

private:
    static void transferCallback(SapXferCallbackInfo* info) {
        auto* self = static_cast<SaperaSdkCaptureSession*>(info->GetContext());
        if (self != nullptr && self->m_callback && self->m_pixels != nullptr) {
            self->m_callback(std::span<const uint16_t>(self->m_pixels, self->m_pixelCount));
        }
    }

    void destroy() {
        if (m_transfer) {
            (void)m_transfer->Destroy();
        }
        if (m_buffers) {
            (void)m_buffers->Destroy();
        }
        if (m_acquisition) {
            (void)m_acquisition->Destroy();
        }
        m_transfer.reset();
        m_buffers.reset();
        m_acquisition.reset();
        m_pixels = nullptr;
        m_pixelCount = 0;
    }

    SaperaConfig m_config;
    FrameCallback m_callback;
    SaperaFrameGeometry m_geometry{};
    std::unique_ptr<SapAcquisition> m_acquisition;
    std::unique_ptr<SapBufferWithTrash> m_buffers;
    std::unique_ptr<SapAcqToBuf> m_transfer;
    const uint16_t* m_pixels = nullptr;
    std::size_t m_pixelCount = 0;
    bool m_running = false;
};
#endif

auto makeDefaultSession(const SaperaConfig& config)
    -> std::unique_ptr<ISaperaCaptureSession> {
#ifdef DSS_HAS_SAPERA
    return std::make_unique<SaperaSdkCaptureSession>(config);
#else
    (void)config;
    return {};
#endif
}

}  // namespace

SaperaFrameSource::SaperaFrameSource(SaperaConfig config,
                                     std::unique_ptr<ISaperaCaptureSession> session,
                                     MessageBus* bus)
    : m_config(std::move(config)), m_session(std::move(session)), m_bus(bus) {}

SaperaFrameSource::~SaperaFrameSource() {
    stop();
}

auto SaperaFrameSource::init() -> std::expected<void, std::string> {
    if (!m_session) {
        m_session = makeDefaultSession(m_config);
    }
    if (!m_session) {
        const std::string message = "Sapera support is disabled in this build";
        reportError(message);
        return std::unexpected(message);
    }

    auto geometry = m_session->initialize(
        [this](std::span<const uint16_t> pixels) { acceptFrame(pixels); });
    if (!geometry) {
        reportError(geometry.error());
        return std::unexpected(geometry.error());
    }
    if (geometry->width == 0 || geometry->height == 0 ||
        geometry->width > std::numeric_limits<std::size_t>::max() / geometry->height) {
        const std::string message = "Sapera returned invalid frame geometry";
        reportError(message);
        return std::unexpected(message);
    }

    std::scoped_lock lock(m_mutex);
    m_width = geometry->width;
    m_height = geometry->height;
    m_frameSeq = 0;
    m_initialized = true;
    return {};
}

auto SaperaFrameSource::startCapture() -> std::expected<void, std::string> {
    bool ready = false;
    {
        std::scoped_lock lock(m_mutex);
        ready = m_initialized && m_session != nullptr;
    }
    if (!ready) {
        const std::string message = "Sapera frame source is not initialized";
        reportError(message);
        return std::unexpected(message);
    }

    auto result = m_session->start();
    if (!result) {
        reportError(result.error());
    }
    return result;
}

void SaperaFrameSource::start() {
    (void)startCapture();
}

void SaperaFrameSource::stop() {
    if (m_session) {
        m_session->stop();
    }
}

void SaperaFrameSource::setFrameCallback(FrameCallback callback) {
    std::scoped_lock lock(m_mutex);
    m_callback = std::move(callback);
}

bool SaperaFrameSource::isRunning() const {
    return m_session && m_session->isRunning();
}

auto SaperaFrameSource::frameWidth() const -> uint32_t {
    std::scoped_lock lock(m_mutex);
    return m_width;
}

auto SaperaFrameSource::frameHeight() const -> uint32_t {
    std::scoped_lock lock(m_mutex);
    return m_height;
}

void SaperaFrameSource::acceptFrame(std::span<const uint16_t> pixels) {
    FrameCallback callback;
    Dss::Processing::FramePacket packet;
    std::string error;
    {
        std::scoped_lock lock(m_mutex);
        const auto expectedCount =
            static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
        if (pixels.size() != expectedCount) {
            error = "Sapera pixel count mismatch: expected " +
                    std::to_string(expectedCount) + ", got " +
                    std::to_string(pixels.size());
        } else {
            callback = m_callback;
            packet.frameSeq = m_frameSeq++;
            packet.width = m_width;
            packet.height = m_height;
            packet.rawImage.assign(pixels.begin(), pixels.end());
        }
    }
    if (!error.empty()) {
        reportError(error);
    } else if (callback) {
        callback(std::move(packet));
    }
}

void SaperaFrameSource::reportError(const std::string& message) const {
    if (m_bus != nullptr) {
        m_bus->emit(Dss::Core::AcquisitionErrorEvent{.source = "sapera", .message = message});
    }
}

}  // namespace Dss::Acquisition