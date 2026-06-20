#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include "dss/acquisition/i_frame_source.h"

namespace Dss::Acquisition {

enum class FrameSourceMode {
    Replay,
    Live,
};

class FrameSourceCoordinator final : public IFrameSource {
public:
    ~FrameSourceCoordinator() override;

    auto registerSource(FrameSourceMode mode, std::shared_ptr<IFrameSource> source)
        -> std::expected<void, std::string>;
    auto selectSource(FrameSourceMode mode) -> std::expected<void, std::string>;
    [[nodiscard]] auto activeMode() const -> std::optional<FrameSourceMode>;

    auto init() -> std::expected<void, std::string> override;
    void start() override;
    void stop() override;
    void setFrameCallback(FrameCallback callback) override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] auto frameWidth() const -> uint32_t override;
    [[nodiscard]] auto frameHeight() const -> uint32_t override;

private:
    [[nodiscard]] auto activeSourceLocked() const -> std::shared_ptr<IFrameSource>;

    mutable std::mutex m_mutex;
    std::map<FrameSourceMode, std::shared_ptr<IFrameSource>> m_sources;
    std::optional<FrameSourceMode> m_activeMode;
    FrameCallback m_callback;
};

}  // namespace Dss::Acquisition
