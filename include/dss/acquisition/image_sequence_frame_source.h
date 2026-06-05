#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dss/acquisition/i_frame_source.h"

namespace Dss::Acquisition {

class ImageSequenceFrameSource final : public IFrameSource {
public:
    ImageSequenceFrameSource();
    explicit ImageSequenceFrameSource(std::vector<std::filesystem::path> files);
    ~ImageSequenceFrameSource() override;

    auto setFiles(std::vector<std::filesystem::path> files) -> std::expected<void, std::string>;
    [[nodiscard]] auto frameCount() const -> std::size_t;
    [[nodiscard]] auto nextFrameIndex() const -> std::size_t;
    void setFrameInterval(std::chrono::milliseconds interval);
    auto stepForward() -> std::expected<void, std::string>;

    auto init() -> std::expected<void, std::string> override;
    void start() override;
    void stop() override;
    void setFrameCallback(FrameCallback callback) override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] auto frameWidth() const -> std::uint32_t override;
    [[nodiscard]] auto frameHeight() const -> std::uint32_t override;

private:
    mutable std::mutex m_mutex;
    std::vector<std::filesystem::path> m_files;
    FrameCallback m_callback;
    std::chrono::milliseconds m_frameInterval{std::chrono::milliseconds{33}};
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::size_t m_nextFrameIndex = 0;
    bool m_initialized = false;

    std::jthread m_worker;
    std::atomic<bool> m_running{false};
};

}  // namespace Dss::Acquisition
